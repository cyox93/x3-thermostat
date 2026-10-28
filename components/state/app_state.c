#include <string.h>
#include "app_state.h"

const uint16_t app_screen_time_steps[APP_SCREEN_TIME_STEPS] = { 10, 20, 30, 60, 120, 300 };

static const char *const s_room_names[APP_ROOM_COUNT] = { "거실", "큰방", "일하는방", "노는방" };
/* Same strings as the HTML simulator's getState().mode (design/TESTING.md) */
static const char *const s_mode_names[APP_MODE_COUNT] = {
    "main", "settings", "edit-room", "edit-brightness", "edit-screenTime",
};

static app_state_t s_state;
static app_state_observer_t s_observer;

static void notify(void)
{
    if (s_observer) s_observer();
}

static int16_t clamp_target(int16_t v)
{
    if (v < APP_TARGET_MIN_X10) return APP_TARGET_MIN_X10;
    if (v > APP_TARGET_MAX_X10) return APP_TARGET_MAX_X10;
    return v;
}

void app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = APP_MODE_MAIN;
    s_state.installed_room = APP_ROOM_LIVING;
    s_state.active_room = APP_ROOM_LIVING;
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        s_state.rooms[i] = (app_room_state_t){
            .on = false, .target_x10 = APP_TARGET_DEFAULT_X10,
            .current_x10 = APP_TEMP_NONE, .humidity = APP_HUM_NONE, .burning = false,
        };
    }
    s_state.rooms[APP_ROOM_LIVING].on = true;
    s_state.room_edit_sel = APP_ROOM_LIVING;
    s_state.brightness = APP_BRIGHTNESS_DEFAULT;
    s_state.screen_time_s = APP_SCREEN_TIME_DEFAULT;
    s_state.wifi_level = APP_WIFI_LEVEL_MAX;
}

const app_state_t *app_state_get(void) { return &s_state; }
bool app_state_is_master(void) { return s_state.installed_room == APP_ROOM_LIVING; }
void app_state_subscribe(app_state_observer_t cb) { s_observer = cb; }

const char *app_room_name(app_room_t room)
{
    return room < APP_ROOM_COUNT ? s_room_names[room] : "?";
}

int app_room_from_name(const char *name)
{
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        if (strcmp(name, s_room_names[i]) == 0) return i;
    }
    return -1;
}

const char *app_mode_name(app_mode_t mode)
{
    return mode < APP_MODE_COUNT ? s_mode_names[mode] : "?";
}

int app_mode_from_name(const char *name)
{
    for (int i = 0; i < APP_MODE_COUNT; i++) {
        if (strcmp(name, s_mode_names[i]) == 0) return i;
    }
    return -1;
}

void app_state_set_mode(app_mode_t mode)
{
    if (mode >= APP_MODE_COUNT) return;
    s_state.mode = mode;
    notify();
}

void app_state_set_installed(app_room_t room)
{
    if (room >= APP_ROOM_COUNT) return;
    s_state.installed_room = room;
    notify();
}

void app_state_set_active(app_room_t room)
{
    if (room >= APP_ROOM_COUNT) return;
    s_state.active_room = room;
    notify();
}

void app_state_set_room(app_room_t room, const app_room_state_t *rs)
{
    if (room >= APP_ROOM_COUNT || !rs) return;
    s_state.rooms[room] = *rs;
    s_state.rooms[room].target_x10 = clamp_target(rs->target_x10);
    notify();
}

void app_state_set_sel(uint8_t sel)
{
    if (sel >= 4) return;
    if (s_state.mode == APP_MODE_EDIT_ROOM) s_state.room_edit_sel = sel;
    else s_state.settings_sel = sel;
    notify();
}

void app_state_set_brightness(uint8_t pct)
{
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    s_state.brightness = pct;
    notify();
}

void app_state_set_screen_time(uint16_t seconds)
{
    for (int i = 0; i < APP_SCREEN_TIME_STEPS; i++) {
        if (app_screen_time_steps[i] == seconds) {
            s_state.screen_time_s = seconds;
            notify();
            return;
        }
    }
}

void app_state_set_wifi(uint8_t level)
{
    s_state.wifi_level = level > APP_WIFI_LEVEL_MAX ? APP_WIFI_LEVEL_MAX : level;
    notify();
}
