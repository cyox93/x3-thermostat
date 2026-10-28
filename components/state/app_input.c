/* The interaction table of design/README.md, in one place. Each function
   handles one input event against app_state and reports what the UI should
   show (flash) and what others need to know (heat / settings changes). */
#include "app_state.h"

#define SETTINGS_ITEMS  4
enum { SET_ROOM, SET_BRIGHTNESS, SET_SCREEN_TIME, SET_BACK };

static app_result_t none(void)
{
    return (app_result_t){ 0 };
}

static app_result_t flash(const char *text, app_accent_t accent)
{
    return (app_result_t){ .flash = text, .accent = accent };
}

static int wrap(int v, int n)
{
    return (v % n + n) % n;
}

static int screen_time_index(uint16_t s)
{
    for (int i = 0; i < APP_SCREEN_TIME_STEPS; i++) {
        if (app_screen_time_steps[i] == s) return i;
    }
    return 2;       /* same fallback as the design (30 s) */
}

app_result_t app_input_enc(int dir)
{
    const app_state_t *st = app_state_get();
    int step = dir > 0 ? 1 : -1;

    switch (st->mode) {
    case APP_MODE_MAIN: {
        app_room_state_t rs = st->rooms[st->active_room];
        if (!rs.on) return none();
        int t = rs.target_x10 + step * APP_TARGET_STEP_X10;
        if (t < APP_TARGET_MIN_X10 || t > APP_TARGET_MAX_X10) return none();
        rs.target_x10 = t;
        app_state_set_room(st->active_room, &rs);
        return (app_result_t){ .heat_changed = true, .heat_room = st->active_room };
    }
    case APP_MODE_SETTINGS:
        app_state_set_sel(wrap(st->settings_sel + step, SETTINGS_ITEMS));
        return none();
    case APP_MODE_EDIT_ROOM:
        app_state_set_sel(wrap(st->room_edit_sel + step, APP_ROOM_COUNT));
        return none();
    case APP_MODE_EDIT_BRIGHTNESS:
        app_state_set_brightness(st->brightness + step * 5);   /* setter clamps 10..100 */
        return none();
    case APP_MODE_EDIT_SCREENTIME: {
        int i = screen_time_index(st->screen_time_s) + step;
        if (i < 0) i = 0;
        if (i >= APP_SCREEN_TIME_STEPS) i = APP_SCREEN_TIME_STEPS - 1;
        app_state_set_screen_time(app_screen_time_steps[i]);
        return none();
    }
    default:
        return none();
    }
}

app_result_t app_input_short(void)
{
    const app_state_t *st = app_state_get();

    switch (st->mode) {
    case APP_MODE_MAIN: {
        app_room_state_t rs = st->rooms[st->active_room];
        rs.on = !rs.on;
        app_state_set_room(st->active_room, &rs);
        app_result_t r = flash(rs.on ? "ON" : "OFF", APP_ACCENT_BOILER);
        r.heat_changed = true;
        r.heat_room = st->active_room;
        return r;
    }
    case APP_MODE_SETTINGS:
        switch (st->settings_sel) {
        case SET_ROOM:
            app_state_set_mode(APP_MODE_EDIT_ROOM);
            app_state_set_sel(st->installed_room);
            return none();
        case SET_BRIGHTNESS:
            app_state_set_mode(APP_MODE_EDIT_BRIGHTNESS);
            return none();
        case SET_SCREEN_TIME:
            app_state_set_mode(APP_MODE_EDIT_SCREENTIME);
            return none();
        default:
            app_state_set_mode(APP_MODE_MAIN);
            return flash("이전", APP_ACCENT_SETTING);
        }
    case APP_MODE_EDIT_ROOM: {
        app_room_t room = st->room_edit_sel;
        app_state_set_installed(room);
        app_state_set_active(room);
        app_state_set_mode(APP_MODE_SETTINGS);
        app_result_t r = flash(app_room_name(room), APP_ACCENT_SETTING);
        r.settings_commit = true;
        return r;
    }
    case APP_MODE_EDIT_BRIGHTNESS:
    case APP_MODE_EDIT_SCREENTIME: {
        app_state_set_mode(APP_MODE_SETTINGS);
        app_result_t r = flash("확인", APP_ACCENT_SETTING);
        r.settings_commit = true;
        return r;
    }
    default:
        return none();
    }
}

app_result_t app_input_long(void)
{
    if (app_state_get()->mode != APP_MODE_MAIN) return none();
    app_state_set_mode(APP_MODE_SETTINGS);
    app_state_set_sel(0);
    return flash("설정", APP_ACCENT_SETTING);
}

app_result_t app_input_room(void)
{
    const app_state_t *st = app_state_get();
    if (st->mode != APP_MODE_MAIN || !app_state_is_master()) return none();
    app_room_t next = (st->active_room + 1) % APP_ROOM_COUNT;
    app_state_set_active(next);
    return flash(app_room_name(next), APP_ACCENT_SETTING);
}
