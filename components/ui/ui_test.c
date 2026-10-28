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
        /* ROOM with arguments sets a room; bare ROOM (the button) comes in stage 2 */
        app_room_state_t rs = {
            .on = atoi(argv[2]) != 0,
            .target_x10 = atoi(argv[3]),
            .current_x10 = none_or_int(argv[4], APP_TEMP_NONE),
            .humidity = none_or_int(argv[5], APP_HUM_NONE),
            .burning = atoi(argv[6]) != 0,
        };
        app_state_set_room(room, &rs);
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
