#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "board.h"
#include "x3switch.h"

/* The longest reply (AT+STATUS?, 6 lines ≈ 70 bytes) takes ~6 ms at 115200;
 * the protocol document recommends 100 ms. */
#define X3SW_TIMEOUT_MS     100
#define X3SW_LINE_MAX       64      /* protocol limit per line */
#define X3SW_RESP_MAX       192
#define X3SW_RX_BUF         512
#define X3SW_UART_QUEUE     16

static const char *TAG = "x3switch";

static SemaphoreHandle_t s_cmd_lock;    /* one transaction at a time */
static SemaphoreHandle_t s_done;        /* reader → sender: OK/ERROR arrived */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* The transaction in flight, shared between sender and reader under s_mux. */
static struct {
    bool active;
    esp_err_t result;
    char buf[X3SW_RESP_MAX];
    size_t used;
} s_txn;

static sc_x3switch_callback_t s_callback;
static void *s_callback_arg;
static QueueHandle_t s_uart_queue;

/* Notifications can arrive at any time, including between the lines of a
 * reply — they must never be mistaken for part of the transaction. Note
 * "+KEY:" is a notification while "+KEYS:" is a reply to AT+KEY?. */
static bool handle_notify(const char *line)
{
    int ch;
    char dir[8];

    if (sscanf(line, "+KEY:%d,%7s", &ch, dir) == 2) {
        if (s_callback) {
            s_callback(strcmp(dir, "DOWN") == 0 ? SC_X3SWITCH_EVENT_KEY_DOWN : SC_X3SWITCH_EVENT_KEY_UP, ch, s_callback_arg);
        }
        return true;
    }

    if (strncmp(line, "+READY:", 7) == 0) {
        ESP_LOGW(TAG, "board ready: %s", line + 7);
        if (s_callback) {
            s_callback(SC_X3SWITCH_EVENT_READY, 0, s_callback_arg);
        }
        return true;
    }

    return false;
}

static void handle_line(const char *line)
{
    if (line[0] == '\0' || handle_notify(line)) {
        return;
    }

    bool finished = false;
    bool orphan = false;

    taskENTER_CRITICAL(&s_mux);
    if (!s_txn.active) {
        orphan = true;
    } else if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) {
        s_txn.result = line[0] == 'O' ? ESP_OK : ESP_FAIL;
        s_txn.active = false;
        finished = true;
    } else {
        size_t n = strlen(line);
        if (s_txn.used + n + 1 < sizeof(s_txn.buf)) {
            memcpy(s_txn.buf + s_txn.used, line, n);
            s_txn.used += n;
            s_txn.buf[s_txn.used++] = '\n';
            s_txn.buf[s_txn.used] = '\0';
        }
    }
    taskEXIT_CRITICAL(&s_mux);

    if (finished) {
        xSemaphoreGive(s_done);
    } else if (orphan) {
        ESP_LOGW(TAG, "unexpected line: %s", line);
    }
}

/* Woken by the UART driver's event queue — no polling. */
static void reader_task(void *arg)
{
    uart_event_t ev;
    uint8_t rx[64];
    char line[X3SW_LINE_MAX + 1];
    size_t len = 0;
    bool overflow = false;

    while (1) {
        if (xQueueReceive(s_uart_queue, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (ev.type == UART_FIFO_OVF || ev.type == UART_BUFFER_FULL) {
            ESP_LOGW(TAG, "rx overflow, dropping input");
            uart_flush_input(BOARD_X3SWITCH_UART_NUM);
            xQueueReset(s_uart_queue);
            len = 0;
            overflow = false;
            continue;
        }
        if (ev.type != UART_DATA) {
            continue;
        }

        size_t remain = ev.size;
        while (remain > 0) {
            int n = uart_read_bytes(BOARD_X3SWITCH_UART_NUM, rx, remain < sizeof(rx) ? remain : sizeof(rx), 0);
            if (n <= 0) {
                break;
            }
            remain -= n;

            for (int i = 0; i < n; i++) {
                char c = (char)rx[i];
                if (c == '\r' || c == '\n') {
                    if (!overflow) {
                        line[len] = '\0';
                        handle_line(line);
                    }
                    len = 0;
                    overflow = false;
                } else if (len < X3SW_LINE_MAX) {
                    line[len++] = c;
                } else {
                    overflow = true;
                }
            }
        }
    }
}

/* Sends one command and collects the "+..." reply lines into resp (may be
 * NULL). Returns ESP_OK on OK, ESP_FAIL on ERROR, ESP_ERR_TIMEOUT otherwise. */
static esp_err_t transact(const char *cmd, char *resp, size_t resp_len)
{
    esp_err_t ret;

    xSemaphoreTake(s_cmd_lock, portMAX_DELAY);

    /* Drop a completion left over from a transaction that timed out just as
     * its OK arrived. */
    xSemaphoreTake(s_done, 0);

    taskENTER_CRITICAL(&s_mux);
    s_txn.active = true;
    s_txn.used = 0;
    s_txn.buf[0] = '\0';
    taskEXIT_CRITICAL(&s_mux);

    uart_write_bytes(BOARD_X3SWITCH_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(BOARD_X3SWITCH_UART_NUM, "\r\n", 2);

    bool got = xSemaphoreTake(s_done, pdMS_TO_TICKS(X3SW_TIMEOUT_MS)) == pdTRUE;

    taskENTER_CRITICAL(&s_mux);
    if (!got && s_txn.active) {
        s_txn.active = false;
        ret = ESP_ERR_TIMEOUT;
    } else {
        ret = s_txn.result;
    }
    if (resp && resp_len) {
        strlcpy(resp, s_txn.buf, resp_len);
    }
    taskEXIT_CRITICAL(&s_mux);

    xSemaphoreGive(s_cmd_lock);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "%s → %s", cmd, ret == ESP_FAIL ? "ERROR" : "timeout");
    }
    return ret;
}

/* Returns the text after "<prefix>" on the matching reply line, or NULL. */
static const char *find_value(const char *resp, const char *prefix)
{
    const char *p = strstr(resp, prefix);
    return p ? p + strlen(prefix) : NULL;
}

static bool valid_ch(int ch)
{
    return ch >= 1 && ch <= SC_X3SWITCH_CH_NUM;
}

esp_err_t sc_x3switch_ping(void)
{
    return transact("AT", NULL, 0);
}

esp_err_t sc_x3switch_reset(void)
{
    return transact("AT+RST", NULL, 0);
}

esp_err_t sc_x3switch_version(char *buf, size_t len)
{
    char resp[X3SW_RESP_MAX];
    ESP_RETURN_ON_ERROR(transact("AT+VER?", resp, sizeof(resp)), TAG, "AT+VER?");

    const char *v = find_value(resp, "+VER:");
    ESP_RETURN_ON_FALSE(v, ESP_ERR_INVALID_RESPONSE, TAG, "no +VER");
    strlcpy(buf, v, len);
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    return ESP_OK;
}

esp_err_t sc_x3switch_status(sc_x3switch_status_t *st)
{
    char resp[X3SW_RESP_MAX];
    int a[4];
    char c[4];
    const char *v;

    ESP_RETURN_ON_ERROR(transact("AT+STATUS?", resp, sizeof(resp)), TAG, "AT+STATUS?");

    v = find_value(resp, "+RELAY:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]) == 4, ESP_ERR_INVALID_RESPONSE, TAG, "+RELAY");
    for (int i = 0; i < 4; i++) st->relay[i] = a[i];

    v = find_value(resp, "+LED:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%c,%c,%c,%c", &c[0], &c[1], &c[2], &c[3]) == 4, ESP_ERR_INVALID_RESPONSE, TAG, "+LED");
    for (int i = 0; i < 4; i++) st->led[i] = (sc_x3switch_led_t)c[i];

    v = find_value(resp, "+LEDIVL:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]) == 4, ESP_ERR_INVALID_RESPONSE, TAG, "+LEDIVL");
    for (int i = 0; i < 4; i++) st->led_interval_ms[i] = a[i];

    v = find_value(resp, "+LEDEN:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%d", &a[0]) == 1, ESP_ERR_INVALID_RESPONSE, TAG, "+LEDEN");
    st->led_enable = a[0];

    v = find_value(resp, "+LEDBRI:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%d", &a[0]) == 1, ESP_ERR_INVALID_RESPONSE, TAG, "+LEDBRI");
    st->led_brightness = a[0];

    v = find_value(resp, "+KEYS:");
    ESP_RETURN_ON_FALSE(v && sscanf(v, "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]) == 4, ESP_ERR_INVALID_RESPONSE, TAG, "+KEYS");
    for (int i = 0; i < 4; i++) st->key[i] = a[i];

    return ESP_OK;
}

esp_err_t sc_x3switch_relay_set(int ch, bool on)
{
    char cmd[24];
    ESP_RETURN_ON_FALSE(valid_ch(ch), ESP_ERR_INVALID_ARG, TAG, "ch %d", ch);
    snprintf(cmd, sizeof(cmd), "AT+RELAY=%d,%d", ch, on ? 1 : 0);
    return transact(cmd, NULL, 0);
}

esp_err_t sc_x3switch_led_set(int ch, sc_x3switch_led_t mode)
{
    char cmd[24];
    ESP_RETURN_ON_FALSE(valid_ch(ch), ESP_ERR_INVALID_ARG, TAG, "ch %d", ch);
    ESP_RETURN_ON_FALSE(mode == SC_X3SWITCH_LED_WHITE || mode == SC_X3SWITCH_LED_ORANGE || mode == SC_X3SWITCH_LED_BLINK,
                        ESP_ERR_INVALID_ARG, TAG, "mode %d", mode);
    snprintf(cmd, sizeof(cmd), "AT+LED=%d,%c", ch, (char)mode);
    return transact(cmd, NULL, 0);
}

esp_err_t sc_x3switch_led_interval(int ch, uint16_t ms)
{
    char cmd[32];
    ESP_RETURN_ON_FALSE(valid_ch(ch), ESP_ERR_INVALID_ARG, TAG, "ch %d", ch);
    ESP_RETURN_ON_FALSE(ms >= 100 && ms <= 60000, ESP_ERR_INVALID_ARG, TAG, "interval %u", ms);
    snprintf(cmd, sizeof(cmd), "AT+LEDIVL=%d,%u", ch, ms);
    return transact(cmd, NULL, 0);
}

esp_err_t sc_x3switch_led_enable(bool on)
{
    return transact(on ? "AT+LEDEN=1" : "AT+LEDEN=0", NULL, 0);
}

esp_err_t sc_x3switch_led_brightness(uint8_t percent)
{
    char cmd[24];
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "brightness %u", percent);
    snprintf(cmd, sizeof(cmd), "AT+LEDBRI=%u", percent);
    return transact(cmd, NULL, 0);
}

void sc_x3switch_register_commands(void);

esp_err_t sc_x3switch_register_callback(sc_x3switch_callback_t callback, void *arg)
{
    s_callback_arg = arg;
    s_callback = callback;
    return ESP_OK;
}

esp_err_t sc_x3switch_init(void)
{
    s_cmd_lock = xSemaphoreCreateMutex();
    s_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_cmd_lock && s_done, ESP_ERR_NO_MEM, TAG, "semaphores");

    const uart_config_t cfg = {
        .baud_rate = BOARD_X3SWITCH_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BOARD_X3SWITCH_UART_NUM, X3SW_RX_BUF, 0, X3SW_UART_QUEUE, &s_uart_queue, 0), TAG, "driver install");
    ESP_RETURN_ON_ERROR(uart_param_config(BOARD_X3SWITCH_UART_NUM, &cfg), TAG, "param config");
    ESP_RETURN_ON_ERROR(uart_set_pin(BOARD_X3SWITCH_UART_NUM, BOARD_X3SWITCH_TX_PIN, BOARD_X3SWITCH_RX_PIN,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "set pin");
    /* Keeps RX idle-high while the FFC is unplugged, so noise does not turn
     * into garbage lines. */
    gpio_pullup_en(BOARD_X3SWITCH_RX_PIN);

    xTaskCreate(reader_task, "x3sw_rx", 3072, NULL, 10, NULL);

    sc_x3switch_register_commands();

    /* The board may be absent (USB-only bench work) — report, don't fail. */
    char ver[32];
    if (sc_x3switch_version(ver, sizeof(ver)) == ESP_OK) {
        ESP_LOGI(TAG, "connected: %s", ver);
    } else {
        ESP_LOGW(TAG, "no response from x3-switch board");
    }

    return ESP_OK;
}
