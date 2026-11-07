#include "esp_log.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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
    ui_screens_create();
    app_state_subscribe(ui_screens_refresh);
    ui_screens_refresh();
    lv_timer_create(_drain, UI_DRAIN_MS, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL %d.%d.%d up", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return ESP_OK;
}
