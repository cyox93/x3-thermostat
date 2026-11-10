#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"

#include "eventloop.h"
#include "config.h"
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
#define SYSLOOP_HEAT_EVENT_ID       5
#define SYSLOOP_PLC_EVENT_ID        6
#define SYSLOOP_BRIGHT_EVENT_ID     7

/* Lights: K3 = L1, K4 = L2 in every room; 큰방 has a third light on K2 (L3).
   Elsewhere K2 is the UI's ROOM key, which only the living room (master)
   acts on. The key's LED shows its light: orange = on, white = off. */
#define _LIGHT_MAX          3
static const int _light_key[_LIGHT_MAX + 1] = { 0, 3, 4, 2 };    /* light → key */

/* Timer ids */
static const int SYSLOOP_ALS_TIMER_ID = 1;
static const int SYSLOOP_TEMP_MEASURE_TIMER_ID = 2;
static const int SYSLOOP_TEMP_GET_TIMER_ID = 3;
static const int SYSLOOP_TEMP_START_TIMER_ID = 4;

/* Room light changes slowly; brightness control filters further anyway */
#define _ALS_PERIOD_MS          1000
/* Room temperature moves slowly; frequent conversions also self-heat the sensor */
#define _TEMP_PERIOD_MS        (1000 * 10)
/* First reading soon after boot so the screen does not wait a whole period */
#define _TEMP_FIRST_MS         2000
#define _TEMP_RETRY_MS         10
#define _TEMP_RETRY_MAX        5

/*
 * Button LED brightness from ambient light, so the unit does not light up the
 * bedroom at night. Two levels: below the threshold the LEDs drop to the dark
 * level, and they come back only above twice the threshold (hysteresis). The
 * threshold and the dark level are set per unit from the console
 * (`bright lux` / `bright led`, docs/architecture.md §3.5) and kept in NVS —
 * no settings screen, by the user's choice. A new level must hold for a while
 * before it is taken: going darker waits longer so a passing shadow does
 * nothing, going brighter is quick so switching the lights on is felt.
 * Lux is the datasheet-typical estimate, not calibrated.
 * The ambient level sets only the button LEDs; the LCD backlight belongs to
 * the UI's brightness setting (docs/ui.md §2.2).
 */
#define _DARK_LUX_DEFAULT       5       /* lights off; a lamp stays bright (user 2026-11-10) */
#define _DARK_LUX_MAX           1000
#define _DARK_LED_DEFAULT       3       /* % — visible without lighting the room */
#define _DARK_EXIT_FACTOR       2
#define _NORMAL_LED             100
#define _BRIGHT_DARKER_HOLD_S   3       /* was 20: too slow when switching the light off (user) */
#define _BRIGHT_BRIGHTER_HOLD_S 1
#define _CFG_DARK_LUX           "bri_dark_lux"
#define _CFG_DARK_LED           "bri_dark_led"

static const char *_bright_names[] = { "dark", "normal" };

/* Console → sysloop: brightness settings change only in the loop thread */
typedef struct _bright_event_data {
    enum { _BRIGHT_SET_FORCE, _BRIGHT_SET_LUX, _BRIGHT_SET_LED } what;
    int value;
} _bright_event_data_t;

/* Key event data for eventloop posting */
typedef struct _room_event_data {
    int room;
    bool heat_on;
} _room_event_data_t;

typedef struct _heat_event_data {
    int room;
    bool on;
    int16_t target_x10;
} _heat_event_data_t;

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
    int dark_lux;                       /* below this the LEDs go dark */
    int dark_led;                       /* button LED % while dark */
    int room;                           /* installed room (app_room_t), from the UI */
    bool heat_on;                       /* heating in the room on screen, from the UI */

    /* Boiler bus (docs/plc-protocol.md §7): the UI's heating per room, what
       the bus says about demand, and our role */
    struct { bool on; int16_t target_x10; bool known; } heat[APP_ROOM_COUNT];
    bool room_demand[APP_ROOM_COUNT];
    bool boiler;
    int plc_addr;                       /* role set on the bus; 0 = not yet */
    bool room_known;                    /* the UI has told us the installed room */
    int ring_burning;                   /* LED ring colour shown: 1 red, 0 white, -1 not set */
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

/**
 * UI heat callback - runs in the LVGL task
 */
static void _ui_heat_cb(int room, bool on, int16_t target_x10, void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    _heat_event_data_t d = { .room = room, .on = on, .target_x10 = target_x10 };
    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_HEAT_EVENT_ID, &d, sizeof(d));
}

/**
 * PLC note callback - runs in the plcloop thread
 */
static void _plc_note_cb(const sc_plc_note_t *note, void *arg)
{
    if (!_sysloop || !_sysloop->loop) return;

    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_PLC_EVENT_ID, (void *)note, sizeof(*note));
}

/* ========== Boiler bus — rooms are app_room_t, a room's address is its index + 1 ========== */

static uint8_t _plc_mode(bool on)
{
    return on ? SC_PROTO_MODE_HEAT : SC_PROTO_MODE_AWAY;   /* off = away: frost-safe at 10 °C */
}

static int _room_of_addr(int addr)
{
    return addr >= 1 && addr <= APP_ROOM_COUNT ? addr - 1 : -1;
}

static bool _is_living(void)
{
    return _sysloop->room == APP_ROOM_LIVING;
}

/* The flame: a room burns while it asks for heat and the boiler runs */
static void _burn_apply(void)
{
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        if (i != _sysloop->room && !_is_living()) continue;
        ui_room_burn_update(i, _sysloop->room_demand[i] && _sysloop->boiler);
    }

    /* The ring shares the backlight line and cannot go dark while the screen
       is on, so its colour tells: red while this unit's room burns, else white */
    int burning = _sysloop->room_demand[_sysloop->room] && _sysloop->boiler;
    if (burning != _sysloop->ring_burning) {
        _sysloop->ring_burning = burning;
        sc_led_color(burning ? 0 : 1);
    }
}

/* (Re)take the installed room's address with its heating as the UI has it */
static void _plc_role_apply(void)
{
    int room = _sysloop->room;
    if (!_sysloop->room_known || !_sysloop->heat[room].known) return;
    int addr = room + 1;
    if (addr == _sysloop->plc_addr) return;
    _sysloop->plc_addr = addr;
    sc_plcloop_set_role(addr, _plc_mode(_sysloop->heat[room].on), _sysloop->heat[room].target_x10 / 5);
}

/* ========== Event handlers — run in the sysloop thread ========== */

/* The user changed a room's heating on the knob, or the UI reports its start */
static void _heat_event_handler(int id, void *arg, void *data)
{
    _heat_event_data_t *d = (_heat_event_data_t *)data;
    if (d->room < 0 || d->room >= APP_ROOM_COUNT) return;

    bool first = !_sysloop->heat[d->room].known;
    _sysloop->heat[d->room].on = d->on;
    _sysloop->heat[d->room].target_x10 = d->target_x10;
    _sysloop->heat[d->room].known = true;
    if (first || !_sysloop->plc_addr) {
        _plc_role_apply();
        return;
    }

    uint8_t mode = _plc_mode(d->on), set2 = d->target_x10 / 5;
    if (d->room == _sysloop->room) {
        sc_plcloop_set_heat(mode, set2);                   /* C5 to the MC200 */
    } else if (_is_living()) {
        sc_plcloop_request_set(d->room + 1, 0x08, mode);   /* F5 → A5 to that room */
        sc_plcloop_request_set(d->room + 1, 0x01, set2);
    }
    ESP_LOGI(TAG, "heat room %d: %s %d.%d", d->room, d->on ? "on" : "off", d->target_x10 / 10, d->target_x10 % 10);
}

static void _plc_event_handler(int id, void *arg, void *data)
{
    sc_plc_note_t *n = (sc_plc_note_t *)data;

    switch (n->kind) {
    case SC_PLC_NOTE_OWN: {                                /* set over the bus: show it */
        int room = _sysloop->room;
        bool on = sc_proto_mode_is_heat(n->mode);
        _sysloop->heat[room].on = on;
        _sysloop->heat[room].target_x10 = n->set2 * 5;
        ui_room_heat_update(room, on, n->set2 * 5);
        ESP_LOGI(TAG, "bus set our room: mode %02X, %d.%d", n->mode, n->set2 / 2, n->set2 % 2 * 5);
    }
        /* our demand may have changed with it */
        __attribute__((fallthrough));
    case SC_PLC_NOTE_STATE:
        _sysloop->boiler = n->boiler;
        _sysloop->room_demand[_sysloop->room] = n->demand;
        _burn_apply();
        break;
    case SC_PLC_NOTE_ROOM: {
        int room = _room_of_addr(n->addr);
        if (room < 0 || room == _sysloop->room) break;
        _sysloop->room_demand[room] = n->demand;
        _sysloop->boiler = n->boiler;
        if (!_is_living()) break;                          /* only the master shows other rooms */
        if (n->temp2 != SC_PROTO_TEMP_NONE) ui_room_climate_update(room, n->temp2 * 5, UI_HUM_NONE);
        if (n->mode >= 0 && n->set2 != SC_PROTO_TEMP_NONE) {
            ui_room_heat_update(room, sc_proto_mode_is_heat(n->mode), n->set2 * 5);
        }
        _burn_apply();
        break;
    }
    }
}

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
    _sysloop->room_known = true;
    _burn_apply();                      /* the ring follows the new room */
    _sysloop->heat_on = d->heat_on;
    _plc_role_apply();
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
    sc_switch_led_brightness(level == SC_BRIGHT_DARK ? _sysloop->dark_led : _NORMAL_LED);
    ui_ambient_dark(level == SC_BRIGHT_DARK);
}

/* Target level for this reading, with hysteresis around the current one */
static sc_bright_level_t _bright_target(float lux, sc_bright_level_t cur)
{
    if (cur == SC_BRIGHT_DARK) {
        return lux > _sysloop->dark_lux * _DARK_EXIT_FACTOR ? SC_BRIGHT_NORMAL : SC_BRIGHT_DARK;
    }
    return lux < _sysloop->dark_lux ? SC_BRIGHT_DARK : SC_BRIGHT_NORMAL;
}

static int _cfg_int(const char *key, int def, int lo, int hi)
{
    char buf[12];
    if (sc_config_get(key, buf, sizeof(buf)) != ESP_OK) return def;
    int v = atoi(buf);
    return v < lo || v > hi ? def : v;
}

static void _cfg_save_int(const char *key, int v)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", v);
    esp_err_t err = sc_config_set(key, buf);
    if (err != ESP_OK) ESP_LOGW(TAG, "save %s: %s", key, esp_err_to_name(err));
}

static void _bright_event_handler(int id, void *arg, void *data)
{
    const _bright_event_data_t *d = data;
    switch (d->what) {
    case _BRIGHT_SET_FORCE:
        _sysloop->bright_force = d->value;  /* applied on the next ALS sample (≤ 1 s) */
        break;
    case _BRIGHT_SET_LUX:
        _sysloop->dark_lux = d->value;
        _cfg_save_int(_CFG_DARK_LUX, d->value);
        ESP_LOGI(TAG, "dark below %d lux (back above %d), saved", d->value, d->value * _DARK_EXIT_FACTOR);
        break;
    case _BRIGHT_SET_LED:
        _sysloop->dark_led = d->value;
        _cfg_save_int(_CFG_DARK_LED, d->value);
        if (_sysloop->info.bright_level == SC_BRIGHT_DARK) sc_switch_led_brightness(d->value);
        ESP_LOGI(TAG, "button LEDs %d %% while dark, saved", d->value);
        break;
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

    if (!present && _sysloop->info.climate_valid) {
        _sysloop->info.climate_valid = false;
        ui_local_climate_update(UI_TEMP_NONE, UI_HUM_NONE);
        sc_plcloop_set_temp(SC_PROTO_TEMP_NONE);       /* no reading, no demand */
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
    ui_local_climate_update((int16_t)lroundf(temp * 10), (int8_t)lroundf(humid));
    sc_plcloop_set_temp((int16_t)lroundf(temp * 2));    /* 0.5 °C steps on the bus */

    /* TODO: heating decision */
    ESP_LOGD(TAG, "climate %.2f C, %.2f %%RH", temp, humid);
}

/**
 * Triggers a measurement and schedules the get, so the loop is never blocked
 * for the ~80 ms the sensor takes.
 */
static void _sysloop_temp_measure_cb(int id, void *arg, void *data);

/* One-shot at _TEMP_FIRST_MS: measure now, then every _TEMP_PERIOD_MS */
static void _sysloop_temp_start_cb(int id, void *arg, void *data)
{
    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_TEMP_MEASURE_TIMER_ID, _sysloop_temp_measure_cb, NULL, _TEMP_PERIOD_MS, true);
    _sysloop_temp_measure_cb(SYSLOOP_TEMP_MEASURE_TIMER_ID, arg, data);
}

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
    printf("bright   : %s%s — button LEDs %d %% (dark below %d lux, back above %d; dark LEDs %d %%)\n",
           _bright_names[info->bright_level], _sysloop->bright_force >= 0 ? " (forced)" : "",
           info->bright_level == SC_BRIGHT_DARK ? _sysloop->dark_led : _NORMAL_LED,
           _sysloop->dark_lux, _sysloop->dark_lux * _DARK_EXIT_FACTOR, _sysloop->dark_led);
    printf("x3switch : %s\n", sc_switch_is_ready() ? "ready" : "not responding");
    return 0;
}

/* bright [auto|dark|normal] · bright lux <n> · bright led <n> */
static struct { struct arg_str *mode; struct arg_int *value; struct arg_end *end; } _bright_args;

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
    bool has_value = _bright_args.value->count > 0;
    int v = has_value ? _bright_args.value->ival[0] : 0;
    _bright_event_data_t d = { .what = _BRIGHT_SET_FORCE, .value = -2 };
    if (!strcmp(m, "lux") || !strcmp(m, "led")) {
        bool lux = !strcmp(m, "lux");
        int hi = lux ? _DARK_LUX_MAX : 100;
        if (!has_value || v < (lux ? 1 : 0) || v > hi) {
            printf("Invalid: bright %s <%d-%d>\n", m, lux ? 1 : 0, hi);
            return 1;
        }
        d = (_bright_event_data_t){ .what = lux ? _BRIGHT_SET_LUX : _BRIGHT_SET_LED, .value = v };
    } else {
        if (!strcmp(m, "auto")) d.value = -1;
        for (int i = 0; i <= SC_BRIGHT_NORMAL; i++) {
            if (!strcmp(m, _bright_names[i])) d.value = i;
        }
        if (d.value == -2) {
            printf("Invalid: '%s' (use auto, dark, normal, lux <n>, led <n>)\n", m);
            return 1;
        }
    }
    sc_eventloop_post_event(_sysloop->loop, SYSLOOP_BRIGHT_EVENT_ID, &d, sizeof(d));
    return 0;
}

static void _sysloop_cmd_init(void)
{
    _bright_args.mode = arg_str0(NULL, NULL, "<auto|dark|normal|lux|led>",
                                 "force a level (bench, no sensor), or set the dark threshold / LED %");
    _bright_args.value = arg_int0(NULL, NULL, "<n>", "lux: dark below n lux (1-1000) · led: button LED % while dark (0-100); saved");
    _bright_args.end = arg_end(2);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "bright", .help = "Button LEDs by ambient light: show, force, or set the dark threshold and level",
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
    _sysloop->dark_lux = _cfg_int(_CFG_DARK_LUX, _DARK_LUX_DEFAULT, 1, _DARK_LUX_MAX);
    _sysloop->dark_led = _cfg_int(_CFG_DARK_LED, _DARK_LED_DEFAULT, 0, 100);
    ESP_LOGI(TAG, "button LEDs %d %% below %d lux", _sysloop->dark_led, _sysloop->dark_lux);
    _sysloop->info.screen_on = true;
    _sysloop->ring_burning = -1;

    /* Handlers do I2C and ADC reads — keep headroom */
    _sysloop->loop = sc_eventloop_create("sysloop", 16, 4096);
    ESP_GOTO_ON_FALSE(_sysloop->loop, ESP_FAIL, _err, TAG, "failed to create sysloop");

    /* Register handlers before the driver callbacks can post */
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_KEY_EVENT_ID, _key_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_ENCODER_EVENT_ID, _encoder_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_SWITCH_READY_EVENT_ID, _switch_ready_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_ROOM_EVENT_ID, _room_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_HEAT_EVENT_ID, _heat_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_PLC_EVENT_ID, _plc_event_handler, NULL);
    sc_eventloop_add_event(_sysloop->loop, SYSLOOP_BRIGHT_EVENT_ID, _bright_event_handler, NULL);

    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_ALS_TIMER_ID, _sysloop_als_cb, NULL, _ALS_PERIOD_MS, true);
    sc_eventloop_add_timer(_sysloop->loop, SYSLOOP_TEMP_START_TIMER_ID, _sysloop_temp_start_cb, NULL, _TEMP_FIRST_MS, false);

    sc_switchloop_register_key_callback(_switch_key_cb, NULL);
    sc_switchloop_register_ready_callback(_switch_ready_cb, NULL);
    /* Heat before room: the role is taken with the installed room's heating */
    ui_register_heat_callback(_ui_heat_cb, NULL);
    ui_register_room_callback(_ui_room_cb, NULL);
    sc_plcloop_register_callback(_plc_note_cb, NULL);
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
    ui_register_heat_callback(NULL, NULL);
    sc_plcloop_register_callback(NULL, NULL);
    sc_encoder_register_callback(NULL, NULL);
    _sysloop_destroy();
}
