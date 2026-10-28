#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "eventloop.h"
#include "x3switch.h"

#include "main.h"

static const char* TAG = "switchloop";

/* Event ids */
#define SWITCHLOOP_APPLY_EVENT_ID   1   /* wanted state changed — send the difference */
#define SWITCHLOOP_KEY_EVENT_ID     2   /* +KEY from the board */
#define SWITCHLOOP_READY_EVENT_ID   3   /* +READY — board rebooted */

/* Timer ids. Long-press timers use SWITCHLOOP_KEY_TIMER_ID + ch. */
static const int SWITCHLOOP_RETRY_TIMER_ID = 1;
#define SWITCHLOOP_KEY_TIMER_ID     10

#define _RETRY_MS           2000
#define _LONG_PRESS_MS      3000    /* long = settings on the UI; short holds must not reach it */

#define _CH_NUM             SC_X3SWITCH_CH_NUM

/* Everything the board can be told. Defaults match the board's own boot state. */
typedef struct _switch_state {
    bool relay[_CH_NUM];
    sc_x3switch_led_t led[_CH_NUM];
    uint16_t led_interval[_CH_NUM];
    bool led_enable;
    uint8_t led_brightness;
} _switch_state_t;

/* Key event data for eventloop posting */
typedef struct _key_event_data {
    int ch;
    bool down;
} _key_event_data_t;

typedef struct _switchloop {
    sc_eventloop_t loop;
    SemaphoreHandle_t mutex;        /* guards want and touched */

    _switch_state_t want;           /* what callers asked for */
    bool touched;                   /* a setter ran before we adopted the board's state */

    /* Loop thread only */
    _switch_state_t board;          /* what the board has confirmed */
    bool board_valid;               /* board holds exactly `board` */
    bool adopted;                   /* initial AT+STATUS? done */
    bool ready;                     /* last exchange succeeded */

    bool key_down[_CH_NUM];
    bool key_long[_CH_NUM];
    sc_switch_key_callback_t key_cb;
    void *key_cb_arg;
} _switchloop_t;

static _switchloop_t *_switchloop;

static void _switch_state_default(_switch_state_t *st)
{
    for (int i = 0; i < _CH_NUM; i++) {
        st->relay[i] = false;
        st->led[i] = SC_X3SWITCH_LED_ORANGE;
        st->led_interval[i] = 1000;
    }
    st->led_enable = true;
    st->led_brightness = 100;
}

static void _schedule_retry(void);

/* ========== Sync — loop thread ========== */

/**
 * Adopt the board's current outputs as the wanted state, so an ESP32 reboot
 * does not switch the lights off behind the user's back.
 */
static bool _switchloop_adopt(void)
{
    sc_x3switch_status_t st;

    if (sc_x3switch_status(&st) != ESP_OK) {
        return false;
    }

    for (int i = 0; i < _CH_NUM; i++) {
        _switchloop->board.relay[i] = st.relay[i];
        _switchloop->board.led[i] = st.led[i];
        _switchloop->board.led_interval[i] = st.led_interval_ms[i];
    }
    _switchloop->board.led_enable = st.led_enable;
    _switchloop->board.led_brightness = st.led_brightness;
    _switchloop->board_valid = true;

    xSemaphoreTake(_switchloop->mutex, portMAX_DELAY);
    if (!_switchloop->touched) {
        _switchloop->want = _switchloop->board;
    }
    xSemaphoreGive(_switchloop->mutex);

    ESP_LOGI(TAG, "adopted board state");
    return true;
}

/*
 * Each field is sent only when it differs from what the board confirmed.
 * A timeout means the board is gone — stop the pass instead of paying
 * 100 ms per remaining command, and retry later.
 */
#define _SEND(cond, call, apply) do {                           \
        if (!valid || (cond)) {                                 \
            esp_err_t _r = (call);                              \
            if (_r == ESP_OK) { apply; }                        \
            else if (_r == ESP_ERR_TIMEOUT) { goto _timeout; }  \
            else { failed = true; }                             \
        }                                                       \
    } while (0)

static void _switchloop_apply(void)
{
    _switch_state_t want;
    _switch_state_t *board = &_switchloop->board;
    bool valid = _switchloop->board_valid;
    bool failed = false;

    if (!_switchloop->adopted) {
        if (!_switchloop_adopt()) goto _timeout;
        _switchloop->adopted = true;
        valid = true;
    }

    xSemaphoreTake(_switchloop->mutex, portMAX_DELAY);
    want = _switchloop->want;
    xSemaphoreGive(_switchloop->mutex);

    for (int i = 0; i < _CH_NUM; i++) {
        _SEND(want.relay[i] != board->relay[i],
              sc_x3switch_relay_set(i + 1, want.relay[i]),
              board->relay[i] = want.relay[i]);
    }

    /* Interval before colour, so a new blink starts at the requested rate */
    for (int i = 0; i < _CH_NUM; i++) {
        _SEND(want.led_interval[i] != board->led_interval[i],
              sc_x3switch_led_interval(i + 1, want.led_interval[i]),
              board->led_interval[i] = want.led_interval[i]);
        _SEND(want.led[i] != board->led[i],
              sc_x3switch_led_set(i + 1, want.led[i]),
              board->led[i] = want.led[i]);
    }

    _SEND(want.led_brightness != board->led_brightness,
          sc_x3switch_led_brightness(want.led_brightness),
          board->led_brightness = want.led_brightness);
    _SEND(want.led_enable != board->led_enable,
          sc_x3switch_led_enable(want.led_enable),
          board->led_enable = want.led_enable);

    _switchloop->ready = true;
    if (failed) {
        /* Board answered ERROR — a value it rejects. Retrying won't help. */
        ESP_LOGW(TAG, "board rejected part of the state");
    } else {
        _switchloop->board_valid = true;
    }
    return;

_timeout:
    if (_switchloop->ready) {
        ESP_LOGW(TAG, "x3-switch not responding");
    }
    _switchloop->ready = false;
    _schedule_retry();
}

#undef _SEND

static void _switchloop_retry_cb(int id, void *arg, void *data)
{
    _switchloop_apply();
}

static void _schedule_retry(void)
{
    sc_eventloop_add_timer(_switchloop->loop, SWITCHLOOP_RETRY_TIMER_ID,
                           _switchloop_retry_cb, NULL, _RETRY_MS, false);
}

static void _apply_event_handler(int id, void *arg, void *data)
{
    _switchloop_apply();
}

static void _ready_event_handler(int id, void *arg, void *data)
{
    /* The board is back at its defaults — push everything we want */
    ESP_LOGI(TAG, "x3-switch rebooted, restoring state");
    _switchloop->board_valid = false;
    _switchloop_apply();
}

/* ========== Keys — loop thread ========== */

static void _key_notify(int ch, sc_switch_key_t event)
{
    ESP_LOGI(TAG, "K%d %s", ch, event == SC_SWITCH_KEY_LONG ? "LONG" : "CLICK");
    if (_switchloop->key_cb) {
        _switchloop->key_cb(ch, event, _switchloop->key_cb_arg);
    }
}

static void _key_long_cb(int id, void *arg, void *data)
{
    int ch = (int)(intptr_t)arg;

    if (_switchloop->key_down[ch - 1]) {
        _switchloop->key_long[ch - 1] = true;
        _key_notify(ch, SC_SWITCH_KEY_LONG);
    }
}

static void _key_event_handler(int id, void *arg, void *data)
{
    _key_event_data_t *key = (_key_event_data_t *)data;
    if (!key || key->ch < 1 || key->ch > _CH_NUM) return;

    int i = key->ch - 1;

    if (key->down) {
        _switchloop->key_down[i] = true;
        _switchloop->key_long[i] = false;
        sc_eventloop_add_timer(_switchloop->loop, SWITCHLOOP_KEY_TIMER_ID + key->ch,
                               _key_long_cb, (void *)(intptr_t)key->ch, _LONG_PRESS_MS, false);
        return;
    }

    if (!_switchloop->key_down[i]) return;

    _switchloop->key_down[i] = false;
    sc_eventloop_del_timer(_switchloop->loop, SWITCHLOOP_KEY_TIMER_ID + key->ch);
    if (!_switchloop->key_long[i]) {
        _key_notify(key->ch, SC_SWITCH_KEY_CLICK);
    }
}

/**
 * x3-switch driver callback - runs in the x3-switch UART reader task
 */
static void _x3switch_driver_cb(sc_x3switch_event_t event, int ch, void *arg)
{
    if (!_switchloop || !_switchloop->loop) return;

    if (event == SC_X3SWITCH_EVENT_READY) {
        sc_eventloop_post_event(_switchloop->loop, SWITCHLOOP_READY_EVENT_ID, NULL, 0);
        return;
    }

    _key_event_data_t key = {
        .ch = ch,
        .down = event == SC_X3SWITCH_EVENT_KEY_DOWN,
    };
    sc_eventloop_post_event(_switchloop->loop, SWITCHLOOP_KEY_EVENT_ID, &key, sizeof(key));
}

/* ========== Public setters — any thread ========== */

#define _CHECK_CH(ch) ESP_RETURN_ON_FALSE((ch) >= 1 && (ch) <= _CH_NUM, ESP_ERR_INVALID_ARG, TAG, "invalid channel %d", (ch))
#define _CHECK_INIT() ESP_RETURN_ON_FALSE(_switchloop && _switchloop->loop, ESP_ERR_INVALID_STATE, TAG, "switchloop not initialized")

static esp_err_t _switch_update(void (*update)(_switch_state_t *st, int ch, int value), int ch, int value)
{
    xSemaphoreTake(_switchloop->mutex, portMAX_DELAY);
    update(&_switchloop->want, ch, value);
    _switchloop->touched = true;
    xSemaphoreGive(_switchloop->mutex);

    return sc_eventloop_post_event(_switchloop->loop, SWITCHLOOP_APPLY_EVENT_ID, NULL, 0);
}

static void _set_relay(_switch_state_t *st, int ch, int v)        { st->relay[ch - 1] = v; }
static void _set_led(_switch_state_t *st, int ch, int v)          { st->led[ch - 1] = (sc_x3switch_led_t)v; }
static void _set_led_interval(_switch_state_t *st, int ch, int v) { st->led_interval[ch - 1] = v; }
static void _set_led_enable(_switch_state_t *st, int ch, int v)   { st->led_enable = v; }
static void _set_led_bri(_switch_state_t *st, int ch, int v)      { st->led_brightness = v; }

esp_err_t sc_switch_relay_set(int ch, bool on)
{
    _CHECK_INIT();
    _CHECK_CH(ch);
    return _switch_update(_set_relay, ch, on);
}

esp_err_t sc_switch_led_set(int ch, sc_x3switch_led_t mode)
{
    _CHECK_INIT();
    _CHECK_CH(ch);
    ESP_RETURN_ON_FALSE(mode == SC_X3SWITCH_LED_WHITE || mode == SC_X3SWITCH_LED_ORANGE || mode == SC_X3SWITCH_LED_BLINK,
                        ESP_ERR_INVALID_ARG, TAG, "invalid mode %d", mode);
    return _switch_update(_set_led, ch, mode);
}

esp_err_t sc_switch_led_interval(int ch, uint16_t ms)
{
    _CHECK_INIT();
    _CHECK_CH(ch);
    ESP_RETURN_ON_FALSE(ms >= 100 && ms <= 60000, ESP_ERR_INVALID_ARG, TAG, "invalid interval %u", ms);
    return _switch_update(_set_led_interval, ch, ms);
}

esp_err_t sc_switch_led_enable(bool on)
{
    _CHECK_INIT();
    return _switch_update(_set_led_enable, 0, on);
}

esp_err_t sc_switch_led_brightness(uint8_t percent)
{
    _CHECK_INIT();
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "invalid brightness %u", percent);
    return _switch_update(_set_led_bri, 0, percent);
}

bool sc_switch_is_ready(void)
{
    return _switchloop && _switchloop->ready;
}

esp_err_t sc_switchloop_register_key_callback(sc_switch_key_callback_t callback, void *arg)
{
    _CHECK_INIT();
    _switchloop->key_cb_arg = arg;
    _switchloop->key_cb = callback;
    return ESP_OK;
}

/* ========== Console ========== */

static const char *_led_name(sc_x3switch_led_t m)
{
    switch (m) {
    case SC_X3SWITCH_LED_WHITE:  return "white";
    case SC_X3SWITCH_LED_ORANGE: return "orange";
    case SC_X3SWITCH_LED_BLINK:  return "blink";
    default:                     return "?";
    }
}

static _switch_state_t _switch_get_want(void)
{
    _switch_state_t st;
    xSemaphoreTake(_switchloop->mutex, portMAX_DELAY);
    st = _switchloop->want;
    xSemaphoreGive(_switchloop->mutex);
    return st;
}

static int _report(esp_err_t err)
{
    if (err != ESP_OK) {
        printf("switch: %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

static bool _parse_on_off(const char *s, bool *on)
{
    if (strcmp(s, "on") == 0) { *on = true; return true; }
    if (strcmp(s, "off") == 0) { *on = false; return true; }
    printf("Invalid: '%s' (use on, off)\n", s);
    return false;
}

/* relay [<ch> <on|off>] */
static struct { struct arg_int *ch; struct arg_str *state; struct arg_end *end; } _relay_args;

static int _relay_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_relay_args) != 0) {
        arg_print_errors(stderr, _relay_args.end, argv[0]);
        return 1;
    }

    if (_relay_args.ch->count == 0) {
        _switch_state_t st = _switch_get_want();
        printf("relay: L1=%d L2=%d L3=%d L4=%d (%s)\n", st.relay[0], st.relay[1], st.relay[2], st.relay[3],
               sc_switch_is_ready() ? "board ready" : "board not responding");
        return 0;
    }

    bool on;
    if (_relay_args.state->count == 0) {
        printf("usage: relay <1-4> <on|off>\n");
        return 1;
    }
    if (!_parse_on_off(_relay_args.state->sval[0], &on)) return 1;

    return _report(sc_switch_relay_set(_relay_args.ch->ival[0], on));
}

/* btn_led [<ch> <white|orange|blink> [<ms>]] */
static struct { struct arg_int *ch; struct arg_str *mode; struct arg_int *ms; struct arg_end *end; } _btn_led_args;

static int _btn_led_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_btn_led_args) != 0) {
        arg_print_errors(stderr, _btn_led_args.end, argv[0]);
        return 1;
    }

    if (_btn_led_args.ch->count == 0) {
        _switch_state_t st = _switch_get_want();
        for (int i = 0; i < _CH_NUM; i++) {
            printf("btn_led K%d: %-6s %u ms\n", i + 1, _led_name(st.led[i]), st.led_interval[i]);
        }
        printf("btn_led: %s, brightness %u %%\n", st.led_enable ? "on" : "off", st.led_brightness);
        return 0;
    }

    if (_btn_led_args.mode->count == 0) {
        printf("usage: btn_led <1-4> <white|orange|blink> [ms]\n");
        return 1;
    }

    int ch = _btn_led_args.ch->ival[0];
    const char *s = _btn_led_args.mode->sval[0];
    sc_x3switch_led_t mode;
    if (strcmp(s, "white") == 0) {
        mode = SC_X3SWITCH_LED_WHITE;
    } else if (strcmp(s, "orange") == 0) {
        mode = SC_X3SWITCH_LED_ORANGE;
    } else if (strcmp(s, "blink") == 0) {
        mode = SC_X3SWITCH_LED_BLINK;
    } else {
        printf("Invalid: '%s' (use white, orange, blink)\n", s);
        return 1;
    }

    if (_btn_led_args.ms->count > 0) {
        if (_report(sc_switch_led_interval(ch, _btn_led_args.ms->ival[0]))) return 1;
    }
    return _report(sc_switch_led_set(ch, mode));
}

/* btn_led_en <on|off> */
static struct { struct arg_str *state; struct arg_end *end; } _btn_led_en_args;

static int _btn_led_en_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_btn_led_en_args) != 0) {
        arg_print_errors(stderr, _btn_led_en_args.end, argv[0]);
        return 1;
    }

    bool on;
    if (!_parse_on_off(_btn_led_en_args.state->sval[0], &on)) return 1;
    return _report(sc_switch_led_enable(on));
}

/* btn_led_bri <0-100> */
static struct { struct arg_int *level; struct arg_end *end; } _btn_led_bri_args;

static int _btn_led_bri_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_btn_led_bri_args) != 0) {
        arg_print_errors(stderr, _btn_led_bri_args.end, argv[0]);
        return 1;
    }

    int level = _btn_led_bri_args.level->ival[0];
    if (level < 0 || level > 100) {
        printf("Invalid: %d (use 0-100)\n", level);
        return 1;
    }
    return _report(sc_switch_led_brightness(level));
}

static void _switchloop_cmd_init(void)
{
    _relay_args.ch = arg_int0(NULL, NULL, "<1-4>", "relay channel");
    _relay_args.state = arg_str0(NULL, NULL, "<on|off>", "contact state");
    _relay_args.end = arg_end(2);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "relay", .help = "Show relays, or switch one on/off",
        .func = &_relay_cmd, .argtable = &_relay_args,
    }));

    _btn_led_args.ch = arg_int0(NULL, NULL, "<1-4>", "button LED channel (K1~K4)");
    _btn_led_args.mode = arg_str0(NULL, NULL, "<white|orange|blink>", "LED colour");
    _btn_led_args.ms = arg_int0(NULL, NULL, "<ms>", "blink interval, 100-60000");
    _btn_led_args.end = arg_end(3);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "btn_led", .help = "Show button LEDs, or set one (no per-channel off: use btn_led_en)",
        .func = &_btn_led_cmd, .argtable = &_btn_led_args,
    }));

    _btn_led_en_args.state = arg_str1(NULL, NULL, "<on|off>", "all button LEDs");
    _btn_led_en_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "btn_led_en", .help = "Turn all button LEDs on/off (keeps colours and brightness)",
        .func = &_btn_led_en_cmd, .argtable = &_btn_led_en_args,
    }));

    _btn_led_bri_args.level = arg_int1(NULL, NULL, "<0-100>", "brightness %");
    _btn_led_bri_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "btn_led_bri", .help = "Set button LED brightness (%)",
        .func = &_btn_led_bri_cmd, .argtable = &_btn_led_bri_args,
    }));
}

/* ========== Init ========== */

static void _switchloop_destroy(void)
{
    if (_switchloop) {
        if (_switchloop->loop) sc_eventloop_destroy(_switchloop->loop);
        if (_switchloop->mutex) vSemaphoreDelete(_switchloop->mutex);
        free(_switchloop);
        _switchloop = NULL;
    }
}

esp_err_t sc_switchloop_init(void)
{
    esp_err_t ret = ESP_OK;

    if (_switchloop) {
        ESP_LOGW(TAG, "switchloop already initialized");
        return ESP_OK;
    }

    _switchloop = (_switchloop_t *)calloc(1, sizeof(_switchloop_t));
    ESP_RETURN_ON_FALSE(_switchloop, ESP_ERR_NO_MEM, TAG, "failed to allocate memory for switchloop");

    _switchloop->mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(_switchloop->mutex, ESP_ERR_NO_MEM, _err, TAG, "failed to create mutex");

    _switch_state_default(&_switchloop->want);
    _switchloop->ready = true;  /* so the first failure is reported */

    _switchloop->loop = sc_eventloop_create("switchloop", 16, 4096);
    ESP_GOTO_ON_FALSE(_switchloop->loop, ESP_FAIL, _err, TAG, "failed to create switchloop");

    sc_eventloop_add_event(_switchloop->loop, SWITCHLOOP_APPLY_EVENT_ID, _apply_event_handler, NULL);
    sc_eventloop_add_event(_switchloop->loop, SWITCHLOOP_KEY_EVENT_ID, _key_event_handler, NULL);
    sc_eventloop_add_event(_switchloop->loop, SWITCHLOOP_READY_EVENT_ID, _ready_event_handler, NULL);

    sc_x3switch_register_callback(_x3switch_driver_cb, NULL);

    _switchloop_cmd_init();

    /* First pass adopts the board's current state (AT+STATUS?) */
    sc_eventloop_post_event(_switchloop->loop, SWITCHLOOP_APPLY_EVENT_ID, NULL, 0);

    return ESP_OK;

_err:
    _switchloop_destroy();

    return ret;
}

void sc_switchloop_end(void)
{
    sc_x3switch_register_callback(NULL, NULL);
    _switchloop_destroy();
}
