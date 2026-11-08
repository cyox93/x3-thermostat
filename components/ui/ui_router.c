#include "esp_log.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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

/* Settings that survive a reboot (NVS): the installed room and the screen
   brightness. Screen time and heating are not stored yet (docs/ui.md stage 3). */
#define UI_CFG_ROOM   "ui_room"
#define UI_CFG_BRIGHT "ui_bright"
static int s_room_saved = -1;
static int s_bright_saved = -1;
static int s_bright_applied = -1;

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
}

/* Saved once confirmed: the installed room only changes when its editor is
   confirmed, and the brightness editor applies live, so wait until it is left */
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
}

/* The UI owns the backlight (docs/ui.md §2.2): 10..100 % on the 8-bit LEDC.
   The LED ring shares the line, so it stays on while the screen is. */
static void _bright_apply(void)
{
    int pct = app_state_get()->brightness;
    if (pct == s_bright_applied) return;
    sc_led_brightness(pct * 255 / 100);
    s_bright_applied = pct;
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

void ui_input(ui_input_t in)
{
    if (!s_in_queue) return;
    if (xQueueSend(s_in_queue, &in, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full, dropped %d", in);
    }
}

static void _drain(lv_timer_t *t)
{
    ui_input_t in;
    while (xQueueReceive(s_in_queue, &in, 0) == pdTRUE) {
        ui_input_apply(in);
    }
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

    s_in_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(ui_input_t));
    ESP_RETURN_ON_FALSE(s_in_queue, ESP_ERR_NO_MEM, TAG, "input queue");

    lvgl_port_lock(0);
    app_state_init();
    _settings_restore();
    _bright_apply();
    ui_screens_create();
    app_state_subscribe(ui_screens_refresh);
    ui_screens_refresh();
    lv_timer_create(_drain, UI_DRAIN_MS, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL %d.%d.%d up", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return ESP_OK;
}
