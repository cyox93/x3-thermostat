#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"

#define SC_LED_OFF   (-1)
#define SC_LED_RED   0
#define SC_LED_WHITE 1

esp_err_t sc_device_init(void);

esp_err_t sc_lcd_init(void);
/* For the UI's LVGL display port. Valid after sc_lcd_init(). */
esp_err_t sc_lcd_get_handles(esp_lcd_panel_io_handle_t *io, esp_lcd_panel_handle_t *panel);

esp_err_t sc_led_init(void);
esp_err_t sc_led_on(bool on);
esp_err_t sc_led_brightness(uint8_t level);
esp_err_t sc_led_color(int color);

esp_err_t sc_gpio_cmd_init(void);

/* One call per detent, dir = +1 (CW) / -1 (CCW). Runs in ISR context —
 * only hand the value off (e.g. sc_eventloop_post_event_from_isr). */
typedef void (*sc_encoder_callback_t)(int dir, void *arg);

esp_err_t sc_encoder_init(void);
esp_err_t sc_encoder_register_callback(sc_encoder_callback_t callback, void *arg);
esp_err_t sc_encoder_get_count(int *value);
esp_err_t sc_encoder_clear(void);

esp_err_t sc_als_init(void);
esp_err_t sc_als_read_mv(int *mv);
/* Approximate lux from the datasheet typical sensitivity. Clamped at the
 * sensor ceiling (BOARD_ALS_LOAD_OHM decides where that is). */
esp_err_t sc_als_read_lux(float *lux);

/* Temperature / humidity: trigger a measurement, then get the result
 * SC_TEMP_HUMID_MEASURE_MS later. get returns ESP_ERR_NOT_FINISHED while the
 * sensor is still converting. temp (°C) or humi (%RH) may be NULL. */
#define SC_TEMP_HUMID_MEASURE_MS    80

esp_err_t sc_temp_humid_init(void);
esp_err_t sc_temp_humid_measure_trigger(void);
esp_err_t sc_temp_humid_get(float *temp, float *humi);
