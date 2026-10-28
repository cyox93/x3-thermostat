#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_priv.h"
#include "ui_geometry.h"

#define MAX_ARGS 8

/* scenarios.json "initial" — and the TEMPS/HUMS constants of
   design/src/golden-states.jsx, which render-sim.mjs repeats */
static void reset_state(void)
{
    static const int16_t temps[APP_ROOM_COUNT] = { 214, 198, 206, 221 };
    static const int8_t hums[APP_ROOM_COUNT] = { 45, 52, 38, 47 };
    app_state_init();
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        app_room_state_t rs = {
            .on = i == APP_ROOM_LIVING, .target_x10 = APP_TARGET_DEFAULT_X10,
            .current_x10 = temps[i], .humidity = hums[i], .burning = i == APP_ROOM_LIVING,
        };
        app_state_set_room(i, &rs);
    }
}

static void reply(ui_test_out_t out, void *arg, const char *s)
{
    out(s, strlen(s), arg);
}

static void snap(ui_test_out_t out, void *arg)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(scr);
    lv_draw_buf_t *db = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);
    if (!db) { reply(out, arg, "ERR snapshot\n"); return; }

    reply(out, arg, "SNAP ");
    /* Row by row (the stride may pad), native little-endian RGB565, base64 */
    char chunk[1024];
    size_t n = 0;
    uint8_t tri[3];
    int k = 0;
    for (int y = 0; y < UIG_H; y++) {
        const uint8_t *row = lv_draw_buf_goto_xy(db, 0, y);
        for (int x = 0; x < UIG_W * 2; x++) {
            tri[k++] = row[x];
            if (k < 3) continue;
            chunk[n++] = b64[tri[0] >> 2];
            chunk[n++] = b64[((tri[0] & 3) << 4) | (tri[1] >> 4)];
            chunk[n++] = b64[((tri[1] & 15) << 2) | (tri[2] >> 6)];
            chunk[n++] = b64[tri[2] & 63];
            k = 0;
            if (n > sizeof(chunk) - 4) { out(chunk, n, arg); n = 0; }
        }
    }
    /* 240 × 240 × 2 = 115200 bytes, a multiple of 3: no padding */
    out(chunk, n, arg);
    reply(out, arg, "\n");
    lv_draw_buf_destroy(db);
}

/* Same keys and units as the HTML simulator's getState() (design/TESTING.md) */
static void dump(ui_test_out_t out, void *arg)
{
    const app_state_t *st = app_state_get();
    char buf[1024];
    char t[3][12];
    int n = snprintf(buf, sizeof(buf),
        "{\"mode\":\"%s\",\"installedRoom\":\"%s\",\"activeRoom\":\"%s\",\"isMaster\":%s,"
        "\"settingsSel\":%d,\"roomEditSel\":\"%s\",\"brightness\":%d,\"screenTime\":%d,\"rooms\":{",
        app_mode_name(st->mode), app_room_name(st->installed_room), app_room_name(st->active_room),
        app_state_is_master() ? "true" : "false", st->settings_sel, app_room_name(st->room_edit_sel),
        st->brightness, st->screen_time_s);
    for (int i = 0; i < APP_ROOM_COUNT && n < (int)sizeof(buf); i++) {
        const app_room_state_t *r = &st->rooms[i];
        ui_fmt_x10(t[0], sizeof(t[0]), r->target_x10);
        if (r->current_x10 == APP_TEMP_NONE) snprintf(t[1], sizeof(t[1]), "null");
        else ui_fmt_x10(t[1], sizeof(t[1]), r->current_x10);
        if (r->humidity == APP_HUM_NONE) snprintf(t[2], sizeof(t[2]), "null");
        else snprintf(t[2], sizeof(t[2]), "%d", r->humidity);
        n += snprintf(buf + n, sizeof(buf) - n,
            "%s\"%s\":{\"on\":%s,\"target\":%s,\"current\":%s,\"humidity\":%s,\"burning\":%s}",
            i ? "," : "", app_room_name(i), r->on ? "true" : "false", t[0], t[1], t[2],
            r->burning ? "true" : "false");
    }
    if (n < (int)sizeof(buf)) snprintf(buf + n, sizeof(buf) - n, "}}\n");
    reply(out, arg, buf);
}

static int none_or_int(const char *s, int none)
{
    return strcmp(s, "-") == 0 ? none : atoi(s);
}

void ui_test_exec(const char *line, ui_test_out_t out, void *arg)
{
    char buf[256];
    char *argv[MAX_ARGS];
    int argc = 0;
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    for (char *t = strtok(buf, " \t\r\n"); t && argc < MAX_ARGS; t = strtok(NULL, " \t\r\n")) argv[argc++] = t;
    if (argc == 0) { reply(out, arg, "ERR empty\n"); return; }

    const char *cmd = argv[0];
    int room = argc >= 2 ? app_room_from_name(argv[1]) : -1;
    if (!strcmp(cmd, "RESET") && argc == 1) {
        reset_state();
    } else if (!strcmp(cmd, "MODE") && argc == 2 && app_mode_from_name(argv[1]) >= 0) {
        app_state_set_mode(app_mode_from_name(argv[1]));
    } else if (!strcmp(cmd, "INSTALL") && argc == 2 && room >= 0) {
        app_state_set_installed(room);
    } else if (!strcmp(cmd, "ACTIVE") && argc == 2 && room >= 0) {
        app_state_set_active(room);
    } else if (!strcmp(cmd, "ROOM") && argc == 7 && room >= 0) {
        /* ROOM with arguments sets a room; bare ROOM is the button */
        app_room_state_t rs = {
            .on = atoi(argv[2]) != 0,
            .target_x10 = atoi(argv[3]),
            .current_x10 = none_or_int(argv[4], APP_TEMP_NONE),
            .humidity = none_or_int(argv[5], APP_HUM_NONE),
            .burning = atoi(argv[6]) != 0,
        };
        app_state_set_room(room, &rs);
    } else if (!strcmp(cmd, "ENC+") && argc == 1) {
        ui_input_apply(UI_IN_ENC_CW);
    } else if (!strcmp(cmd, "ENC-") && argc == 1) {
        ui_input_apply(UI_IN_ENC_CCW);
    } else if (!strcmp(cmd, "SHORT") && argc == 1) {
        ui_input_apply(UI_IN_SHORT);
    } else if (!strcmp(cmd, "LONG") && argc == 1) {
        ui_input_apply(UI_IN_LONG);
    } else if (!strcmp(cmd, "ROOM") && argc == 1) {
        ui_input_apply(UI_IN_ROOM);
    } else if (!strcmp(cmd, "BURN") && argc == 3 && room >= 0) {
        app_room_state_t rs = app_state_get()->rooms[room];
        rs.burning = atoi(argv[2]) != 0;
        app_state_set_room(room, &rs);
    } else if (!strcmp(cmd, "SEL") && argc == 2) {
        app_state_set_sel(atoi(argv[1]));
    } else if (!strcmp(cmd, "BRIGHT") && argc == 2) {
        app_state_set_brightness(atoi(argv[1]));
    } else if (!strcmp(cmd, "STIME") && argc == 2) {
        app_state_set_screen_time(atoi(argv[1]));
    } else if (!strcmp(cmd, "DUMP") && argc == 1) {
        dump(out, arg);
        return;
    } else if (!strcmp(cmd, "WIFI") && argc == 2) {
        app_state_set_wifi(atoi(argv[1]));
    } else if (!strcmp(cmd, "ANIM") && argc == 2) {
        ui_screens_set_anim(atoi(argv[1]) != 0);
    } else if (!strcmp(cmd, "SNAP") && argc == 1) {
        snap(out, arg);
        return;
    } else {
        reply(out, arg, "ERR unknown\n");
        return;
    }
    reply(out, arg, "OK\n");
}
