#include "esp_log.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "device.h"
#include "ui.h"
#include "app_state.h"
#include "ui_priv.h"

static const char *TAG = "ui";

#define UI_LCD_W 240
#define UI_LCD_H 240
#define UI_QUEUE_LEN 16
#define UI_DRAIN_MS  10

/* app_state and LVGL objects change only in the LVGL task; other threads hand
   inputs over through this queue, drained by an lv_timer (lv_async_call is not
   safe from other threads without the lock). */
static QueueHandle_t s_in_queue;

typedef struct {
    enum { UI_MSG_INPUT, UI_MSG_LOCAL_CLIMATE, UI_MSG_ROOM_CLIMATE, UI_MSG_ROOM_BURN, UI_MSG_ROOM_HEAT } kind;
    ui_input_t in;
    int8_t room;
    bool flag;                          /* burning, or heat on */
    int16_t temp_x10;                   /* or heat target */
    int8_t humidity;
} ui_msg_t;

static void _post(const ui_msg_t *m)
{
    if (!s_in_queue) return;
    if (xQueueSend(s_in_queue, m, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, dropped message %d", m->kind);
    }
}

/* Settings that survive a reboot (NVS): the installed room, the screen
   brightness and time, and the installed room's heating */
#define UI_CFG_ROOM   "ui_room"
#define UI_CFG_BRIGHT "ui_bright"
#define UI_CFG_STIME  "ui_stime"
#define UI_CFG_HEAT   "ui_heat"           /* installed room: "<on> <target ×10>" */
static int s_room_saved = -1;
static int s_bright_saved = -1;
static int s_stime_saved = -1;
static int s_duty_applied = -1;

/* Screen time: after it the LCD and the LED ring go off together (duty 0 on
   their shared line). Duty 1/255 would keep the ring lit with the LCD looking
   off (bench 2026-11-10), but the user wants the ring off too; "항상" keeps
   both on. */
#define UI_DARK_DUTY 0
static bool s_dark;
static uint32_t s_input_ms;             /* last input, lv_tick */

/* Going dark and waking fade instead of switching: the eye sees brightness
   about as duty^(1/2.2), so the fade runs on that scale or it would look like
   it drops off at once at the end. The ring only reacts on/off, so it still
   goes out when the duty reaches 0. Editing the brightness stays immediate. */
#define UI_FADE_OFF_MS 500
#define UI_FADE_ON_MS  200
#define UI_GAMMA       2.2f
static bool s_fading;
static uint32_t s_fade_ms;              /* fade start, lv_tick */
static float s_fade_from;               /* perceived level 0..1 */
static float s_level;                   /* perceived level now */

static bool _cfg_get_int(const char *key, int *v)
{
    char buf[8];
    if (sc_config_get(key, buf, sizeof(buf)) != ESP_OK) return false;
    *v = atoi(buf);
    return true;
}

static bool _cfg_set_int(const char *key, int v)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", v);
    esp_err_t err = sc_config_set(key, buf);
    if (err != ESP_OK) ESP_LOGW(TAG, "save %s: %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}

static void _heat_restore(void)
{
    char buf[16];
    if (sc_config_get(UI_CFG_HEAT, buf, sizeof(buf)) != ESP_OK) return;
    int on, target;
    if (sscanf(buf, "%d %d", &on, &target) != 2) return;
    const app_state_t *st = app_state_get();
    app_room_state_t rs = st->rooms[st->installed_room];
    rs.on = on;
    rs.target_x10 = target;
    app_state_set_room(st->installed_room, &rs);
    ESP_LOGI(TAG, "heating %s %d.%d (saved)", on ? "on" : "off", target / 10, target % 10);
}

static void _heat_save(bool on, int16_t target_x10)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %d", on, target_x10);
    esp_err_t err = sc_config_set(UI_CFG_HEAT, buf);
    if (err != ESP_OK) ESP_LOGW(TAG, "save heat: %s", esp_err_to_name(err));
}

static void _settings_restore(void)
{
    int v;
    if (_cfg_get_int(UI_CFG_ROOM, &v) && v >= 0 && v < APP_ROOM_COUNT) {
        app_state_set_installed(v);
        app_state_set_active(v);
        s_room_saved = v;
        ESP_LOGI(TAG, "installed room %s (saved)", app_room_name(v));
    }
    if (_cfg_get_int(UI_CFG_BRIGHT, &v)) {
        app_state_set_brightness(v);        /* the setter clamps 10..100 */
        s_bright_saved = app_state_get()->brightness;
        ESP_LOGI(TAG, "brightness %d %% (saved)", s_bright_saved);
    }
    if (_cfg_get_int(UI_CFG_STIME, &v)) {
        app_state_set_screen_time(v);       /* ignored unless one of the steps */
        s_stime_saved = app_state_get()->screen_time_s;
        ESP_LOGI(TAG, "screen time %d s (saved, 0 = always)", s_stime_saved);
    }
    _heat_restore();
}

/* Saved once confirmed: the installed room only changes when its editor is
   confirmed; the brightness and screen-time editors change the value on every
   step, so wait until they are left */
static void _settings_save(void)
{
    const app_state_t *st = app_state_get();
    if (st->installed_room != s_room_saved && _cfg_set_int(UI_CFG_ROOM, st->installed_room)) {
        s_room_saved = st->installed_room;
        ESP_LOGI(TAG, "installed room %s saved", app_room_name(st->installed_room));
    }
    if (st->mode != APP_MODE_EDIT_BRIGHTNESS && st->brightness != s_bright_saved &&
        _cfg_set_int(UI_CFG_BRIGHT, st->brightness)) {
        s_bright_saved = st->brightness;
        ESP_LOGI(TAG, "brightness %d %% saved", st->brightness);
    }
    if (st->mode != APP_MODE_EDIT_SCREENTIME && st->screen_time_s != s_stime_saved &&
        _cfg_set_int(UI_CFG_STIME, st->screen_time_s)) {
        s_stime_saved = st->screen_time_s;
        ESP_LOGI(TAG, "screen time %d s saved", st->screen_time_s);
    }
}

/* The UI owns the backlight (docs/ui.md §2.2): 10..100 % on the 8-bit LEDC,
   UI_DARK_DUTY while dark. The LED ring shares the line: on unless dark. */
static float _level_of(int duty) { return powf(duty / 255.0f, 1.0f / UI_GAMMA); }
static int _duty_of(float level) { return (int)lroundf(255.0f * powf(level, UI_GAMMA)); }

static void _fade_start(void)
{
    s_fading = true;
    s_fade_ms = lv_tick_get();
    s_fade_from = s_level;
}

static void _bright_apply(void)
{
    int duty = s_dark ? UI_DARK_DUTY : app_state_get()->brightness * 255 / 100;
    if (s_fading) {
        float t = (float)lv_tick_elaps(s_fade_ms) / (s_dark ? UI_FADE_OFF_MS : UI_FADE_ON_MS);
        if (t >= 1.0f) {
            s_fading = false;
        } else {
            float e = t * t * (3.0f - 2.0f * t);        /* smoothstep */
            duty = _duty_of(s_fade_from + (_level_of(duty) - s_fade_from) * e);
        }
    }
    if (duty == s_duty_applied) return;
    sc_led_brightness(duty);
    s_duty_applied = duty;
    s_level = _level_of(duty);
}

/* Every input restarts the screen time. On a dark screen the first input only
   wakes it (docs/ui.md §2.2): true = the input is used up. */
static bool _wake(void)
{
    s_input_ms = lv_tick_get();
    if (!s_dark) return false;
    s_dark = false;
    _fade_start();
    ESP_LOGI(TAG, "screen on");
    return true;
}

/* The mode and any value being edited stay as they are while dark */
static void _screen_time_check(void)
{
    uint16_t sec = app_state_get()->screen_time_s;
    if (s_dark || sec == APP_SCREEN_TIME_ALWAYS) return;
    if (lv_tick_elaps(s_input_ms) < (uint32_t)sec * 1000) return;
    s_dark = true;
    _fade_start();
    ESP_LOGI(TAG, "screen off after %d s", sec);
}

static ui_room_callback_t s_room_cb;
static void *s_room_cb_arg;
static int s_room_reported = -1;
static int s_heat_reported = -1;

void ui_register_room_callback(ui_room_callback_t cb, void *arg)
{
    s_room_cb_arg = arg;
    s_room_cb = cb;
}

/* Sysloop picks the light keys by room and lights K1 by heating, so tell
   it both */
static void _report_room(void)
{
    ui_room_callback_t cb = s_room_cb;
    const app_state_t *st = app_state_get();
    int room = st->installed_room;
    int heat = st->rooms[st->active_room].on;
    if (cb && (room != s_room_reported || heat != s_heat_reported)) {
        s_room_reported = room;
        s_heat_reported = heat;
        cb(room, heat, s_room_cb_arg);
    }
}

/* ===== Heat: report knob changes once quiet, take bus changes silently ===== */

#define UI_HEAT_QUIET_MS 300

static ui_heat_callback_t s_heat_cb;
static void *s_heat_cb_arg;
static struct {
    bool on;
    int16_t target_x10;
    bool reported;                      /* a value has been reported since start */
    bool pending;                       /* differs from the reported value */
    uint32_t changed_ms;                /* last change while pending */
} s_heat[APP_ROOM_COUNT];

void ui_register_heat_callback(ui_heat_callback_t cb, void *arg)
{
    s_heat_cb_arg = arg;
    s_heat_cb = cb;
}

static void _heat_report(void)
{
    ui_heat_callback_t cb = s_heat_cb;
    if (!cb) return;
    const app_state_t *st = app_state_get();
    uint32_t now = lv_tick_get();
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        const app_room_state_t *r = &st->rooms[i];
        bool differs = !s_heat[i].reported || r->on != s_heat[i].on || r->target_x10 != s_heat[i].target_x10;
        if (!differs) {
            s_heat[i].pending = false;
            continue;
        }
        if (!s_heat[i].pending) {
            s_heat[i].pending = true;
            s_heat[i].changed_ms = now;
        }
        /* The first report goes out at once; knob turns wait to settle */
        if (s_heat[i].reported && now - s_heat[i].changed_ms < UI_HEAT_QUIET_MS) continue;
        bool first = !s_heat[i].reported;
        s_heat[i].on = r->on;
        s_heat[i].target_x10 = r->target_x10;
        s_heat[i].reported = true;
        s_heat[i].pending = false;
        cb(i, r->on, r->target_x10, s_heat_cb_arg);
        if (!first && i == (int)st->installed_room) _heat_save(r->on, r->target_x10);
    }
}

/* Any change to the state while pending restarts the quiet time */
static void _heat_touch(void)
{
    const app_state_t *st = app_state_get();
    uint32_t now = lv_tick_get();
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        const app_room_state_t *r = &st->rooms[i];
        if (s_heat[i].reported && (r->on != s_heat[i].on || r->target_x10 != s_heat[i].target_x10)) {
            s_heat[i].pending = true;
            s_heat[i].changed_ms = now;
        }
    }
}

static void _room_heat_apply(int room, bool on, int16_t target_x10)
{
    if (room < 0 || room >= APP_ROOM_COUNT || s_heat[room].pending) return;
    const app_state_t *st = app_state_get();
    app_room_state_t rs = st->rooms[room];
    if (rs.on == on && rs.target_x10 == target_x10) return;
    rs.on = on;
    rs.target_x10 = target_x10;
    app_state_set_room(room, &rs);
    /* Already true on the bus: never report it back */
    s_heat[room].on = on;
    s_heat[room].target_x10 = target_x10;
    s_heat[room].reported = true;
    if (room == (int)st->installed_room) _heat_save(on, target_x10);
}

static void _room_climate_apply(int room, int16_t temp_x10, int8_t humidity)
{
    if (room < 0 || room >= APP_ROOM_COUNT) return;
    app_room_state_t rs = app_state_get()->rooms[room];
    rs.current_x10 = temp_x10 == UI_TEMP_NONE ? APP_TEMP_NONE : temp_x10;
    rs.humidity = humidity == UI_HUM_NONE ? APP_HUM_NONE : humidity;
    app_state_set_room(room, &rs);
}

static void _room_burn_apply(int room, bool burning)
{
    if (room < 0 || room >= APP_ROOM_COUNT) return;
    app_room_state_t rs = app_state_get()->rooms[room];
    if (rs.burning == burning) return;
    rs.burning = burning;
    app_state_set_room(room, &rs);
}

void ui_room_climate_update(int room, int16_t temp_x10, int8_t humidity)
{
    _post(&(ui_msg_t){ .kind = UI_MSG_ROOM_CLIMATE, .room = room, .temp_x10 = temp_x10, .humidity = humidity });
}

void ui_room_burn_update(int room, bool burning)
{
    _post(&(ui_msg_t){ .kind = UI_MSG_ROOM_BURN, .room = room, .flag = burning });
}

void ui_room_heat_update(int room, bool on, int16_t target_x10)
{
    _post(&(ui_msg_t){ .kind = UI_MSG_ROOM_HEAT, .room = room, .flag = on, .temp_x10 = target_x10 });
}

void ui_input(ui_input_t in)
{
    _post(&(ui_msg_t){ .kind = UI_MSG_INPUT, .in = in });
}

void ui_local_climate_update(int16_t temp_x10, int8_t humidity)
{
    _post(&(ui_msg_t){ .kind = UI_MSG_LOCAL_CLIMATE, .temp_x10 = temp_x10, .humidity = humidity });
}

/* The sensor belongs to whichever room the unit is installed in at the time */
static void _local_climate_apply(int16_t temp_x10, int8_t humidity)
{
    const app_state_t *st = app_state_get();
    app_room_state_t rs = st->rooms[st->installed_room];
    rs.current_x10 = temp_x10 == UI_TEMP_NONE ? APP_TEMP_NONE : temp_x10;
    rs.humidity = humidity == UI_HUM_NONE ? APP_HUM_NONE : humidity;
    app_state_set_room(st->installed_room, &rs);
}

static void _drain(lv_timer_t *t)
{
    ui_msg_t m;
    while (xQueueReceive(s_in_queue, &m, 0) == pdTRUE) {
        switch (m.kind) {
        case UI_MSG_INPUT:
            if (_wake()) break;
            ui_input_apply(m.in);
            _heat_touch();
            break;
        case UI_MSG_LOCAL_CLIMATE: _local_climate_apply(m.temp_x10, m.humidity); break;
        case UI_MSG_ROOM_CLIMATE:  _room_climate_apply(m.room, m.temp_x10, m.humidity); break;
        case UI_MSG_ROOM_BURN:     _room_burn_apply(m.room, m.flag); break;
        case UI_MSG_ROOM_HEAT:     _room_heat_apply(m.room, m.flag, m.temp_x10); break;
        }
    }
    _heat_report();
    _screen_time_check();
    _bright_apply();
    _settings_save();
    _report_room();
}

esp_err_t ui_init(void)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    ESP_RETURN_ON_ERROR(sc_lcd_get_handles(&io, &panel), TAG, "lcd not ready");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16384;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = UI_LCD_W * UI_LCD_H / 10,
        .double_buffer = true,
        .hres = UI_LCD_W,
        .vres = UI_LCD_H,
        .color_format = LV_COLOR_FORMAT_RGB565,
        /* The X3 panel is mounted with rows running bottom to top (MADCTL MY,
         * 0x88). The port rewrites the panel's mirror bits from this at start. */
        .rotation = { .mirror_y = true },
        /* The panel takes big-endian pixels and no longer swaps itself */
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "add_disp");

    s_in_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(ui_msg_t));
    ESP_RETURN_ON_FALSE(s_in_queue, ESP_ERR_NO_MEM, TAG, "input queue");

    lvgl_port_lock(0);
    app_state_init();
    _settings_restore();
    s_input_ms = lv_tick_get();
    _bright_apply();
    ui_screens_create();
    app_state_subscribe(ui_screens_refresh);
    ui_screens_refresh();
    lv_timer_create(_drain, UI_DRAIN_MS, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL %d.%d.%d up", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return ESP_OK;
}
