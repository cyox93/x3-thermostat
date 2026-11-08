#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"

#include "eventloop.h"
#include "device.h"
#include "ui.h"
#include "app_state.h"

#include "main.h"

static const char* TAG = "sysloop";

/* Event ids — posted by callbacks, handled in the sysloop thread */
#define SYSLOOP_KEY_EVENT_ID        1
#define SYSLOOP_ENCODER_EVENT_ID    2
#define SYSLOOP_SWITCH_READY_EVENT_ID 3
#define SYSLOOP_ROOM_EVENT_ID       4

/* Lights: K3 = L1, K4 = L2 in every room; 큰방 has a third light on K2 (L3).
   Elsewhere K2 is the UI's ROOM key, which only the living room (master)
   acts on. The key's LED shows its light: orange = on, white = off. */
#define _LIGHT_MAX          3
static const int _light_key[_LIGHT_MAX + 1] = { 0, 3, 4, 2 };    /* light → key */

/* Timer ids */
static const int SYSLOOP_ALS_TIMER_ID = 1;
static const int SYSLOOP_TEMP_MEASURE_TIMER_ID = 2;
static const int SYSLOOP_TEMP_GET_TIMER_ID = 3;

/* Room light changes slowly; brightness control filters further anyway */
#define _ALS_PERIOD_MS          1000
/* Room temperature moves slowly; frequent conversions also self-heat the sensor */
#define _TEMP_PERIOD_MS        (1000 * 10)
#define _TEMP_RETRY_MS         10
#define _TEMP_RETRY_MAX        5

/*
 * Brightness levels from ambient light. The goal is a unit that does not
 * light up the bedroom at night, so the thresholds sit at the dark end:
 *   < 3 lux   lights off (street light through curtains is ~1~5 lux)
 *   3~50 lux  bedside lamp / indirect light
 *   > 50 lux  room lights on (ceiling light is typically 100~300 lux)
 * Each boundary has hysteresis, and a new level must hold for a while
 * before it is taken: going darker waits longer so a passing shadow does
 * nothing, going brighter is quick so switching the lights on is felt.
 * Lux is the datasheet-typical estimate — tune these on the bench.
 */
#define _LUX_NIGHT_ENTER        3.0f
#define _LUX_NIGHT_EXIT         8.0f
#define _LUX_DIM_ENTER          30.0f
#define _LUX_DIM_EXIT           60.0f
#define _BRIGHT_DARKER_HOLD_S   20
#define _BRIGHT_BRIGHTER_HOLD_S 3

/* The ambient level sets only the button LEDs; the LCD backlight belongs to
   the UI's brightness setting (docs/ui.md §2.2) */
typedef struct _bright_output {
    uint8_t btn_led;    /* x3-switch button LEDs, % */
} _bright_output_t;

static const _bright_output_t _bright_outputs[] = {
    [SC_BRIGHT_NIGHT]  = { .btn_led = 3   },
    [SC_BRIGHT_DIM]    = { .btn_led = 30  },
    [SC_BRIGHT_NORMAL] = { .btn_led = 100 },
};

static const char *_bright_names[] = { "night", "dim", "normal" };

/* Key event data for eventloop posting */
typedef struct _room_event_data {
    int room;
    bool heat_on;
} _room_event_data_t;

typedef struct _key_event_data {
    int ch;             /* 1..4 = K1..K4 */
    sc_switch_key_t event;
} _key_event_data_t;

typedef struct _sysloop {
    sc_eventloop_t loop;

    sc_sysinfo_t info;

    int temp_retry;
    bool temp_present;

    sc_bright_level_t bright_pending;   /* candidate level while it holds */
    int bright_hold;                    /* seconds the candidate has held */
    int bright_force;                   /* -1 = auto, else forced level */
    int room;                           /* installed room (app_room_t), from the UI */
    bool heat_on;                       /* heating in the room on screen, from the UI */
} _sysloop_t;

static _sysloop_t *_sysloop;

/* Forward declarations */

/* ========== Driver callbacks — hand off to the sysloop thread ========== */

/**
 * Key callback - runs in the switchloop thread
 */
static void _switch_key_cb(int ch, sc_switch_key_t event, void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    _key_event_data_t key_data = {
        .ch = ch,
        .event = event,
    };
    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_KEY_EVENT_ID, &key_data, sizeof(key_data));
}

/**
 * Encoder callback - runs in ISR context
 */
static void _encoder_driver_cb(int dir, void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    sc_eventloop_post_event_from_isr(_sysloop->loop, SYSLOOP_ENCODER_EVENT_ID, &dir, sizeof(dir));
}

/**
 * Switch ready callback - runs in the switchloop thread
 */
/**
 * UI room callback - runs in the LVGL task
 */
static void _ui_room_cb(int room, bool heat_on, void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    _room_event_data_t d = { .room = room, .heat_on = heat_on };
    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_ROOM_EVENT_ID, &d, sizeof(d));
}

static void _switch_ready_cb(void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_SWITCH_READY_EVENT_ID, NULL, 0);
}

/* ========== Event handlers — run in the sysloop thread ========== */

static int _light_num(void)
{
    return _sysloop->room == APP_ROOM_BIG ? 3 : 2;
}

static int _light_of_key(int key)
{
    for (int light = 1; light <= _light_num(); light++) {
        if (_light_key[light] == key) return light;
    }
    return 0;
}

static void _light_led_apply(int light)
{
    bool on = sc_switch_relay_get(light);
    sc_switch_led_set(_light_key[light], on ? SC_X3SWITCH_LED_ORANGE : SC_X3SWITCH_LED_WHITE);
}

/* K1 switches heating: its LED shows it, orange = on, white = off. K2 is a
   light in 큰방, the ROOM key (orange) in the living room, and white where
   it does nothing. */
static void _key_leds_apply(void)
{
    sc_switch_led_set(1, _sysloop->heat_on ? SC_X3SWITCH_LED_ORANGE : SC_X3SWITCH_LED_WHITE);
    if (_sysloop->room != APP_ROOM_BIG) {
        sc_switch_led_set(2, _sysloop->room == APP_ROOM_LIVING ? SC_X3SWITCH_LED_ORANGE : SC_X3SWITCH_LED_WHITE);
    }
    for (int light = 1; light <= _light_num(); light++) {
        _light_led_apply(light);
    }
}

static void _light_toggle(int light)
{
    bool on = !sc_switch_relay_get(light);
    sc_switch_relay_set(light, on);
    _light_led_apply(light);
    ESP_LOGI(TAG, "light %d %s", light, on ? "on" : "off");
}

/* The board's relays were adopted (they survive an ESP32 reboot): make the
   key LEDs show them */
static void _switch_ready_event_handler(int id, void *arg, void *data)
{
    _key_leds_apply();
}

static void _room_event_handler(int id, void *arg, void *data)
{
    _room_event_data_t *d = (_room_event_data_t *)data;
    int room = d->room;
    _sysloop->room = room;
    _sysloop->heat_on = d->heat_on;
    if (sc_switch_is_ready()) {
        _key_leds_apply();
    }
    ESP_LOGI(TAG, "room %d: %d lights, heating %s", room, _light_num(), d->heat_on ? "on" : "off");
}

static void _key_event_handler(int id, void *arg, void *data)
{
    _key_event_data_t *key_data = (_key_event_data_t *)data;
    if (!key_data) return;

    /* K1 is the push the knob lacks (docs/ui.md §2.1); see _light_key */
    bool lng = key_data->event == SC_SWITCH_KEY_LONG;
    int light = _light_of_key(key_data->ch);
    if (key_data->ch == 1) ui_input(lng ? UI_IN_LONG : UI_IN_SHORT);
    else if (light && !lng) _light_toggle(light);
    else if (key_data->ch == 2 && !lng) ui_input(UI_IN_ROOM);
    ESP_LOGI(TAG, "K%d %s", key_data->ch, lng ? "LONG" : "CLICK");
}

static void _encoder_event_handler(int id, void *arg, void *data)
{
    int dir = *(int *)data;

    ui_input(dir > 0 ? UI_IN_ENC_CW : UI_IN_ENC_CCW);
    ESP_LOGD(TAG, "encoder %s", dir > 0 ? "CW" : "CCW");
}

/* ========== Brightness ========== */

static void _bright_set_level(sc_bright_level_t level)
{
    if (level == _sysloop->info.bright_level) return;

    ESP_LOGI(TAG, "brightness %s -> %s (~%.0f lux)", _bright_names[_sysloop->info.bright_level],
             _bright_names[level], _sysloop->info.ambient_lux);

    _sysloop->info.bright_level = level;
    sc_switch_led_brightness(_bright_outputs[level].btn_led);
}

/* Target level for this reading, with hysteresis around the current one */
static sc_bright_level_t _bright_target(float lux, sc_bright_level_t cur)
{
    switch (cur) {
    case SC_BRIGHT_NIGHT:
        if (lux > _LUX_DIM_EXIT) return SC_BRIGHT_NORMAL;
        if (lux > _LUX_NIGHT_EXIT) return SC_BRIGHT_DIM;
        return SC_BRIGHT_NIGHT;
    case SC_BRIGHT_DIM:
        if (lux > _LUX_DIM_EXIT) return SC_BRIGHT_NORMAL;
        if (lux < _LUX_NIGHT_ENTER) return SC_BRIGHT_NIGHT;
        return SC_BRIGHT_DIM;
    default:
        if (lux < _LUX_NIGHT_ENTER) return SC_BRIGHT_NIGHT;
        if (lux < _LUX_DIM_ENTER) return SC_BRIGHT_DIM;
        return SC_BRIGHT_NORMAL;
    }
}

/* Called once per ALS sample (1 s) */
static void _bright_update(float lux)
{
    sc_bright_level_t cur = _sysloop->info.bright_level;

    if (_sysloop->bright_force >= 0) {
        _bright_set_level(_sysloop->bright_force);
        return;
    }

    sc_bright_level_t target = _bright_target(lux, cur);
    if (target == cur) {
        _sysloop->bright_hold = 0;
        return;
    }

    if (target != _sysloop->bright_pending) {
        _sysloop->bright_pending = target;
        _sysloop->bright_hold = 0;
    }

    int need = target < cur ? _BRIGHT_DARKER_HOLD_S : _BRIGHT_BRIGHTER_HOLD_S;
    if (++_sysloop->bright_hold >= need) {
        _sysloop->bright_hold = 0;
        _bright_set_level(target);
    }
}

/* ========== Periodic sensors ========== */

static void _sysloop_als_cb(int id, void *arg, void *data)
{
    float lux;

    if (sc_als_read_lux(&lux) == ESP_OK) {
        _sysloop->info.ambient_lux = lux;
        ESP_LOGD(TAG, "ambient ~%.0f lux", lux);
        _bright_update(lux);
    }
}

static void _temp_set_present(bool present)
{
    /* Log only on change so an unplugged sensor does not flood the console */
    if (present != _sysloop->temp_present) {
        _sysloop->temp_present = present;
        if (present) {
            ESP_LOGI(TAG, "temp/humid sensor responding");
        } else {
            ESP_LOGW(TAG, "temp/humid sensor not responding");
        }
    }

    if (!present) {
        _sysloop->info.climate_valid = false;
    }
}

static void _sysloop_temp_get_cb(int id, void *arg, void *data)
{
    float temp, humid;
    esp_err_t ret = sc_temp_humid_get(&temp, &humid);

    if (ret == ESP_ERR_NOT_FINISHED && _sysloop->temp_retry++ < _TEMP_RETRY_MAX) {
        sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_TEMP_GET_TIMER_ID,
                               _sysloop_temp_get_cb, NULL, _TEMP_RETRY_MS, false);
        return;
    }

    _temp_set_present(ret == ESP_OK);
    if (ret != ESP_OK) return;

    _sysloop->info.temp = temp;
    _sysloop->info.humid = humid;
    _sysloop->info.climate_valid = true;

    /* TODO: heating decision */
    ESP_LOGD(TAG, "climate %.2f C, %.2f %%RH", temp, humid);
}

/**
 * Triggers a measurement and schedules the get, so the loop is never blocked
 * for the ~80 ms the sensor takes.
 */
static void _sysloop_temp_measure_cb(int id, void *arg, void *data)
{
    if (sc_temp_humid_measure_trigger() != ESP_OK) {
        _temp_set_present(false);
        return;
    }

    _sysloop->temp_retry = 0;
    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_TEMP_GET_TIMER_ID,
                           _sysloop_temp_get_cb, NULL, SC_TEMP_HUMID_MEASURE_MS, false);
}

/* ========== Console ========== */

static int _sysinfo_cmd(int argc, char **argv)
{
    const sc_sysinfo_t *info = sc_sysloop_get_info();
    if (!info) {
        printf("sysloop not initialized\n");
        return 1;
    }

    if (info->climate_valid) {
        printf("climate  : %.2f C, %.2f %%RH\n", info->temp, info->humid);
    } else {
        printf("climate  : -\n");
    }
    printf("ambient  : ~%.0f lux\n", info->ambient_lux);
    printf("bright   : %s%s, screen %s\n", _bright_names[info->bright_level],
           _sysloop->bright_force >= 0 ? " (forced)" : "", info->screen_on ? "on" : "off");
    printf("x3switch : %s\n", sc_switch_is_ready() ? "ready" : "not responding");
    return 0;
}

/* bright [auto|night|dim|normal] */
static struct { struct arg_str *mode; struct arg_end *end; } _bright_args;

static int _bright_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_bright_args) != 0) {
        arg_print_errors(stderr, _bright_args.end, argv[0]);
        return 1;
    }

    if (_bright_args.mode->count == 0) {
        return _sysinfo_cmd(0, NULL);
    }

    const char *m = _bright_args.mode->sval[0];
    int force = -2;
    if (strcmp(m, "auto") == 0) force = -1;
    for (int i = 0; i <= SC_BRIGHT_NORMAL; i++) {
        if (strcmp(m, _bright_names[i]) == 0) force = i;
    }
    if (force == -2) {
        printf("Invalid: '%s' (use auto, night, dim, normal)\n", m);
        return 1;
    }

    /* Takes effect on the next ALS sample (≤ 1 s), in the sysloop thread */
    _sysloop->bright_force = force;
    return 0;
}

static void _sysloop_cmd_init(void)
{
    _bright_args.mode = arg_str0(NULL, NULL, "<auto|night|dim|normal>", "force a level (bench, no sensor)");
    _bright_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "bright", .help = "Show or force the ambient brightness level (button LEDs)",
        .func = &_bright_cmd, .argtable = &_bright_args,
    }));

    const esp_console_cmd_t sysinfo_cmd = {
        .command = "sysinfo",
        .help = "Show state held by sysloop",
        .hint = NULL,
        .func = &_sysinfo_cmd,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&sysinfo_cmd));
}

/* ========== Init ========== */

static void _sysloop_destroy(void)
{
    if (_sysloop) {
        if (_sysloop->loop) sc_eventloop_destroy(_sysloop->loop);
        free(_sysloop);
        _sysloop = NULL;
    }
}

const sc_sysinfo_t *sc_sysloop_get_info(void)
{
    return _sysloop ? &_sysloop->info : NULL;
}

esp_err_t sc_sysloop_init(void)
{
    esp_err_t ret = ESP_OK;

    if (_sysloop) {
        ESP_LOGW(TAG, "sysloop already initialized");
        return ESP_OK;
    }

    _sysloop = (_sysloop_t *)calloc(1, sizeof(_sysloop_t));
    ESP_RETURN_ON_FALSE(_sysloop, ESP_ERR_NO_MEM, TAG, "failed to allocate memory for sysloop");

    /* Assume present so the first failure is reported */
    _sysloop->temp_present = true;

    /* Start bright; the first readings take it down if the room is dark */
    _sysloop->info.bright_level = SC_BRIGHT_NORMAL;
    _sysloop->bright_pending = SC_BRIGHT_NORMAL;
    _sysloop->bright_force = -1;
    _sysloop->info.screen_on = true;

    /* Handlers do I2C and ADC reads — keep headroom */
    _sysloop->loop = sc_eventloop_create("sysloop", 16, 4096);
    ESP_GOTO_ON_FALSE(_sysloop->loop, ESP_FAIL, _err, TAG, "failed to create sysloop");

    /* Register handlers before the driver callbacks can post */
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_KEY_EVENT_ID, _key_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_ENCODER_EVENT_ID, _encoder_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_SWITCH_READY_EVENT_ID, _switch_ready_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_ROOM_EVENT_ID, _room_event_handler, NULL);

    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_ALS_TIMER_ID, _sysloop_als_cb, NULL, _ALS_PERIOD_MS, true);
    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_TEMP_MEASURE_TIMER_ID, _sysloop_temp_measure_cb, NULL, _TEMP_PERIOD_MS, true);

    sc_switchloop_register_key_callback(_switch_key_cb, NULL);
    sc_switchloop_register_ready_callback(_switch_ready_cb, NULL);
    ui_register_room_callback(_ui_room_cb, NULL);
    sc_encoder_register_callback(_encoder_driver_cb, NULL);

    _sysloop_cmd_init();

    return ESP_OK;

_err:
    _sysloop_destroy();

    return ret;
}

void sc_sysloop_end(void)
{
    sc_switchloop_register_key_callback(NULL, NULL);
    sc_switchloop_register_ready_callback(NULL, NULL);
    ui_register_room_callback(NULL, NULL);
    sc_encoder_register_callback(NULL, NULL);
    _sysloop_destroy();
}
