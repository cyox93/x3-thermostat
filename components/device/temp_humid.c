/*
 * Temperature / humidity sensor — AHT25 (Aosong) on I2C. Callers see only the
 * sc_temp_humid_* API; the chip is an implementation detail of this file.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "driver/i2c_master.h"
#include "board.h"
#include "device.h"

#define AHT25_ADDR          0x38
#define AHT25_I2C_HZ        100000
#define AHT25_TIMEOUT_MS    50
#define AHT25_BUSY_RETRIES  5
#define AHT25_BUSY_RETRY_MS 10

#define AHT25_STATUS_BUSY   0x80
/* Both bits must be set after power-on, otherwise registers 0x1B/0x1C/0x1E
 * need the vendor re-initialisation sequence (Aosong AHT2x sample code). */
#define AHT25_STATUS_CAL    0x18

static const char *TAG = "temp_humid";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_lock;
static bool s_ready;

static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
        }
    }
    return crc;
}

static esp_err_t read_status(uint8_t *status)
{
    return i2c_master_transmit_receive(s_dev, (uint8_t[]){0x71}, 1, status, 1, AHT25_TIMEOUT_MS);
}

static esp_err_t reset_reg(uint8_t reg)
{
    uint8_t buf[3];

    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_dev, (uint8_t[]){reg, 0x00, 0x00}, 3, AHT25_TIMEOUT_MS), TAG, "reg 0x%02x write", reg);
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(i2c_master_receive(s_dev, buf, 3, AHT25_TIMEOUT_MS), TAG, "reg 0x%02x read", reg);
    vTaskDelay(pdMS_TO_TICKS(10));
    return i2c_master_transmit(s_dev, (uint8_t[]){0xB0 | reg, buf[1], buf[2]}, 3, AHT25_TIMEOUT_MS);
}

/* Done lazily on first read so the unit still boots with no sensor attached
 * (USB-only bench work). */
static esp_err_t ensure_ready(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(read_status(&status), TAG, "no response at 0x%02x", AHT25_ADDR);

    if ((status & AHT25_STATUS_CAL) != AHT25_STATUS_CAL) {
        ESP_LOGW(TAG, "status 0x%02x, re-initialising registers", status);
        ESP_RETURN_ON_ERROR(reset_reg(0x1B), TAG, "reset 0x1B");
        ESP_RETURN_ON_ERROR(reset_reg(0x1C), TAG, "reset 0x1C");
        ESP_RETURN_ON_ERROR(reset_reg(0x1E), TAG, "reset 0x1E");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    s_ready = true;
    return ESP_OK;
}

esp_err_t sc_temp_humid_measure_trigger(void)
{
    esp_err_t ret;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    ret = ensure_ready();
    if (ret == ESP_OK) {
        ret = i2c_master_transmit(s_dev, (uint8_t[]){0xAC, 0x33, 0x00}, 3, AHT25_TIMEOUT_MS);
    }
    if (ret != ESP_OK) {
        /* Force a status check next time — the sensor may have been
         * unplugged or power-cycled. */
        s_ready = false;
    }
    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t sc_temp_humid_get(float *temp, float *humi)
{
    esp_err_t ret = ESP_OK;
    uint8_t d[7];

    xSemaphoreTake(s_lock, portMAX_DELAY);

    ESP_GOTO_ON_ERROR(i2c_master_receive(s_dev, d, sizeof(d), AHT25_TIMEOUT_MS), end, TAG, "read failed");
    if (d[0] & AHT25_STATUS_BUSY) {
        ret = ESP_ERR_NOT_FINISHED;
        goto end;
    }
    ESP_GOTO_ON_FALSE(crc8(d, 6) == d[6], ESP_ERR_INVALID_CRC, end, TAG, "crc mismatch");

    uint32_t raw_h = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t raw_t = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

    if (temp) {
        *temp = raw_t * 200.0f / (1 << 20) - 50.0f;
    }
    if (humi) {
        *humi = raw_h * 100.0f / (1 << 20);
    }

end:
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FINISHED) {
        s_ready = false;
    }
    xSemaphoreGive(s_lock);
    return ret;
}

static int _temp_get_cmd(int argc, char **argv)
{
    float temp, humi;
    esp_err_t err = sc_temp_humid_measure_trigger();

    for (int i = 0; err == ESP_OK; i++) {
        vTaskDelay(pdMS_TO_TICKS(i == 0 ? SC_TEMP_HUMID_MEASURE_MS : AHT25_BUSY_RETRY_MS));
        err = sc_temp_humid_get(&temp, &humi);
        if (err != ESP_ERR_NOT_FINISHED || i >= AHT25_BUSY_RETRIES) {
            break;
        }
    }

    if (err != ESP_OK) {
        printf("temp_get: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("temperature : %.02f, humidity : %.02f\n", temp, humi);
    return 0;
}

esp_err_t sc_temp_humid_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "mutex");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = BOARD_I2C_SDA_PIN,
        .scl_io_num = BOARD_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        /* Board R14/R15 (4.7 kΩ) are the real pull-ups; the internal ones
         * only keep the lines defined when those are not fitted. */
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "new bus failed");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT25_ADDR,
        .scl_speed_hz = AHT25_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), TAG, "add device failed");

    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "temp_get", .help = "Get temperature and humidity sensor",
        .func = &_temp_get_cmd,
    }));

    ESP_LOGI(TAG, "AHT25 initialized (SDA=GPIO%d, SCL=GPIO%d)", BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
    return ESP_OK;
}
