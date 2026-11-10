#pragma once
#include <stdbool.h>
#include <stdint.h>

/* UI state — the single source the screens draw from (docs/ui.md §3.2).
   Pure C: built for ESP-IDF and for the PC sim. Not thread-safe: only the
   UI thread (LVGL task / sim main loop) calls these. */

typedef enum {
    APP_ROOM_LIVING,        /* 거실 — installing here makes the unit the master */
    APP_ROOM_BIG,           /* 큰방 */
    APP_ROOM_SMALL,         /* 작은방 */
    APP_ROOM_WORK,          /* 일하는방 */
    APP_ROOM_COUNT,
} app_room_t;

typedef enum {
    APP_MODE_MAIN,
    APP_MODE_SETTINGS,
    APP_MODE_EDIT_ROOM,
    APP_MODE_EDIT_BRIGHTNESS,
    APP_MODE_EDIT_SCREENTIME,
    APP_MODE_COUNT,
} app_mode_t;

/* Temperatures are ×10 (225 = 22.5 °C) so drawing needs no floats */
#define APP_TARGET_MIN_X10      150
#define APP_TARGET_MAX_X10      300
#define APP_TARGET_STEP_X10       5
#define APP_TARGET_DEFAULT_X10  220
#define APP_TEMP_NONE           INT16_MIN
#define APP_HUM_NONE            (-1)

#define APP_BRIGHTNESS_DEFAULT   70
#define APP_SCREEN_TIME_DEFAULT  30
#define APP_WIFI_LEVEL_MAX        3
#define APP_SCREEN_TIME_STEPS     6

typedef struct {
    bool    on;
    int16_t target_x10;
    int16_t current_x10;    /* APP_TEMP_NONE = no reading */
    int8_t  humidity;       /* APP_HUM_NONE = no reading */
    bool    burning;        /* this room's heating demand */
} app_room_state_t;

typedef struct {
    app_mode_t mode;
    app_room_t installed_room;
    app_room_t active_room;
    app_room_state_t rooms[APP_ROOM_COUNT];
    uint8_t    settings_sel;    /* 0..3 */
    app_room_t room_edit_sel;
    uint8_t    brightness;      /* 10..100 % */
    uint16_t   screen_time_s;   /* one of app_screen_time_steps */
    uint8_t    wifi_level;      /* 0..3 */
} app_state_t;

extern const uint16_t app_screen_time_steps[APP_SCREEN_TIME_STEPS];   /* 10 20 30 60 120 300 */

typedef void (*app_state_observer_t)(void);

/* Boot defaults (docs/ui.md §3.4): installed 거실 on at 22.0, other rooms
   off at 22.0 with no readings, brightness 70, screen time 30 s, WiFi 3. */
void app_state_init(void);
const app_state_t *app_state_get(void);
bool app_state_is_master(void);

/* Single observer, called after every change. NULL detaches. */
void app_state_subscribe(app_state_observer_t cb);

const char *app_room_name(app_room_t room);
/* -1 when the name is not one of the four rooms */
int app_room_from_name(const char *name);
/* Same strings as the HTML simulator's getState().mode; -1 if unknown */
const char *app_mode_name(app_mode_t mode);
int app_mode_from_name(const char *name);

/* Raw setters — no interaction rules. Used by boot, the test commands and
   outside updates. Out-of-range values are clamped or ignored. */
void app_state_set_mode(app_mode_t mode);
void app_state_set_installed(app_room_t room);
void app_state_set_active(app_room_t room);
void app_state_set_room(app_room_t room, const app_room_state_t *rs);
void app_state_set_sel(uint8_t sel);
void app_state_set_brightness(uint8_t pct);
void app_state_set_screen_time(uint16_t seconds);
void app_state_set_wifi(uint8_t level);

/* ─── Interaction (app_input.c) — design/README.md "Interaction Table" ───
   One call per input event. The state changes here; what the UI should show
   or tell others comes back in the result, so this file never draws. */
typedef enum {
    APP_ACCENT_BOILER,          /* ON / OFF */
    APP_ACCENT_SETTING,         /* 설정 · 이전 · 확인 · room names */
} app_accent_t;

typedef struct {
    const char  *flash;         /* press-flash text, NULL = none */
    app_accent_t accent;
    bool heat_changed;          /* on/target of heat_room changed */
    app_room_t heat_room;
    bool settings_commit;       /* installed room / brightness / screen time confirmed */
} app_result_t;

app_result_t app_input_enc(int dir);    /* +1 clockwise, -1 counter-clockwise */
app_result_t app_input_short(void);
app_result_t app_input_long(void);
app_result_t app_input_room(void);      /* ROOM button (K2) — ignored unless master */
