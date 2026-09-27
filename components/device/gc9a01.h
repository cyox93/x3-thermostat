#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_dev.h"
#include "board.h"

#define GC9A01_LCD_WIDTH  240
#define GC9A01_LCD_HEIGHT 240

#define GC9A01_PIN_MOSI  BOARD_LCD_PIN_MOSI
#define GC9A01_PIN_SCLK  BOARD_LCD_PIN_SCLK
#define GC9A01_PIN_CS    BOARD_LCD_PIN_CS
#define GC9A01_PIN_DC    BOARD_LCD_PIN_DC
#define GC9A01_PIN_RST   BOARD_LCD_PIN_RST

#define GC9A01_SPI_HOST  BOARD_LCD_SPI_HOST
#define GC9A01_SPI_CLOCK_HZ (40 * 1000 * 1000)

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
} gc9a01_handle_t;

esp_err_t esp_lcd_new_panel_gc9a01(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *config,
                                    esp_lcd_panel_handle_t *ret_panel);

esp_err_t gc9a01_init(gc9a01_handle_t *handle);
esp_err_t gc9a01_set_backlight(bool on);
esp_err_t gc9a01_fill_color(gc9a01_handle_t *handle, uint16_t color);
esp_err_t gc9a01_draw_bitmap(gc9a01_handle_t *handle, int x_start, int y_start, int x_end, int y_end, const void *color_data);

uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b);
