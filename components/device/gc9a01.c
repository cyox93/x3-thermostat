#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_private/gpio.h"
#include "esp_rom_sys.h"
#include "soc/spi_pins.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "gc9a01.h"
#include "device.h"

static const char *TAG = "gc9a01";

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int x_gap;
    int y_gap;
    uint8_t madctl_val;
} gc9a01_panel_t;

static esp_err_t gc9a01_panel_del(esp_lcd_panel_t *panel);
static esp_err_t gc9a01_panel_reset(esp_lcd_panel_t *panel);
static esp_err_t gc9a01_panel_init(esp_lcd_panel_t *panel);
static esp_err_t gc9a01_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t gc9a01_panel_invert_color(esp_lcd_panel_t *panel, bool invert);
static esp_err_t gc9a01_panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t gc9a01_panel_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t gc9a01_panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t gc9a01_panel_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

static void gc9a01_send_vendor_init(esp_lcd_panel_io_handle_t io)
{
    // Extended init sequence for GC9A01
    static const struct { uint8_t cmd; uint8_t data[16]; uint8_t len; } cmds[] = {
        {0xEF, {0}, 0},
        {0xEB, {0x14}, 1},
        {0xFE, {0}, 0},
        {0xEF, {0}, 0},
        {0xEB, {0x14}, 1},
        {0x84, {0x40}, 1},
        {0x85, {0xFF}, 1},
        {0x86, {0xFF}, 1},
        {0x87, {0xFF}, 1},
        {0x88, {0x0A}, 1},
        {0x89, {0x21}, 1},
        {0x8A, {0x00}, 1},
        {0x8B, {0x80}, 1},
        {0x8C, {0x01}, 1},
        {0x8D, {0x01}, 1},
        {0x8E, {0xFF}, 1},
        {0x8F, {0xFF}, 1},
        {0xB6, {0x00, 0x00}, 2},
        {0x90, {0x08, 0x08, 0x08, 0x08}, 4},
        {0xBD, {0x06}, 1}, {0xBC, {0x00}, 1},
        {0xFF, {0x60, 0x01, 0x04}, 3},
        {0xC3, {0x13}, 1}, {0xC4, {0x13}, 1},
        {0xC9, {0x22}, 1}, {0xBE, {0x11}, 1},
        {0xE1, {0x10, 0x0E}, 2},
        {0xDF, {0x21, 0x0C, 0x02}, 3},
        {0xF0, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6},
        {0xF1, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6},
        {0xF2, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6},
        {0xF3, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6},
        {0xED, {0x1B, 0x0B}, 2},
        {0xAE, {0x77}, 1}, {0xCD, {0x63}, 1},
        {0x70, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9},
        {0xE8, {0x34}, 1},
        {0x62, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12},
        {0x63, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12},
        {0x64, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7},
        {0x66, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10},
        {0x67, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10},
        {0x74, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7},
        {0x98, {0x3E, 0x07}, 2},
    };

    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_lcd_panel_io_tx_param(io, cmds[i].cmd, cmds[i].data, cmds[i].len);
    }
}

esp_err_t esp_lcd_new_panel_gc9a01(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    gc9a01_panel_t *gc9a01 = NULL;
    ESP_GOTO_ON_FALSE(io && config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    gc9a01 = calloc(1, sizeof(gc9a01_panel_t));
    ESP_GOTO_ON_FALSE(gc9a01, ESP_ERR_NO_MEM, err, TAG, "no mem for gc9a01 panel");

    gc9a01->madctl_val = 0;
    if (config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR) {
        gc9a01->madctl_val |= LCD_CMD_BGR_BIT;
    }

    gc9a01->io = io;
    gc9a01->base.del = gc9a01_panel_del;
    gc9a01->base.reset = gc9a01_panel_reset;
    gc9a01->base.init = gc9a01_panel_init;
    gc9a01->base.draw_bitmap = gc9a01_panel_draw_bitmap;
    gc9a01->base.invert_color = gc9a01_panel_invert_color;
    gc9a01->base.set_gap = gc9a01_panel_set_gap;
    gc9a01->base.mirror = gc9a01_panel_mirror;
    gc9a01->base.swap_xy = gc9a01_panel_swap_xy;
    gc9a01->base.disp_on_off = gc9a01_panel_disp_on_off;
    *ret_panel = &gc9a01->base;

    ESP_LOGI(TAG, "new gc9a01 panel @%p", gc9a01);
    return ESP_OK;

err:
    free(gc9a01);
    return ret;
}

static esp_err_t gc9a01_panel_del(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    free(gc9a01);
    return ESP_OK;
}

static esp_err_t gc9a01_panel_reset(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_tx_param(gc9a01->io, LCD_CMD_SWRESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static esp_err_t gc9a01_panel_init(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;

    gc9a01_send_vendor_init(io);

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &gc9a01->madctl_val, 1), TAG, "MADCTL failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]){0x05}, 1), TAG, "COLMOD failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x35, NULL, 0), TAG, "TE ON failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_INVON, NULL, 0), TAG, "INVON failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "SLPOUT failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_DISPON, NULL, 0), TAG, "DISPON failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "GC9A01 initialized (%dx%d)", GC9A01_LCD_WIDTH, GC9A01_LCD_HEIGHT);
    return ESP_OK;
}

static esp_err_t gc9a01_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;

    x_start += gc9a01->x_gap;
    x_end += gc9a01->x_gap;
    y_start += gc9a01->y_gap;
    y_end += gc9a01->y_gap;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF, x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF,
    }, 4), TAG, "CASET failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF, y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF,
    }, 4), TAG, "RASET failed");

    // GC9A01 expects big-endian RGB565 — swap bytes on little-endian ESP32
    size_t pixel_count = (x_end - x_start) * (y_end - y_start);
    size_t len = pixel_count * 2;
    uint16_t *swapped = heap_caps_malloc(len, MALLOC_CAP_DMA);
    if (swapped) {
        const uint16_t *src = color_data;
        for (size_t i = 0; i < pixel_count; i++) {
            swapped[i] = __builtin_bswap16(src[i]);
        }
        esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, swapped, len);
        free(swapped);
    } else {
        esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);
    }

    return ESP_OK;
}

static esp_err_t gc9a01_panel_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_tx_param(gc9a01->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
    return ESP_OK;
}

static esp_err_t gc9a01_panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    if (mirror_x) gc9a01->madctl_val |= LCD_CMD_MX_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MX_BIT;
    if (mirror_y) gc9a01->madctl_val |= LCD_CMD_MY_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MY_BIT;
    esp_lcd_panel_io_tx_param(gc9a01->io, LCD_CMD_MADCTL, &gc9a01->madctl_val, 1);
    return ESP_OK;
}

static esp_err_t gc9a01_panel_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    if (swap_axes) gc9a01->madctl_val |= LCD_CMD_MV_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MV_BIT;
    esp_lcd_panel_io_tx_param(gc9a01->io, LCD_CMD_MADCTL, &gc9a01->madctl_val, 1);
    return ESP_OK;
}

static esp_err_t gc9a01_panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    gc9a01->x_gap = x_gap;
    gc9a01->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t gc9a01_panel_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_tx_param(gc9a01->io, on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
    return ESP_OK;
}

// --- Public convenience API ---

esp_err_t gc9a01_init(gc9a01_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing SPI bus");

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GC9A01_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = GC9A01_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_LCD_WIDTH * GC9A01_LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(GC9A01_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = GC9A01_PIN_DC,
        .cs_gpio_num = GC9A01_PIN_CS,
        .pclk_hz = GC9A01_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(GC9A01_SPI_HOST, &io_cfg, &handle->io));

    // Hardware reset
    gpio_config_t rst_cfg = {
        .pin_bit_mask = 1ULL << GC9A01_PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(GC9A01_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GC9A01_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(handle->io, &panel_cfg, &handle->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(handle->panel));

    return ESP_OK;
}

esp_err_t gc9a01_fill_color(gc9a01_handle_t *handle, uint16_t color)
{
    uint16_t *buf = heap_caps_malloc(GC9A01_LCD_WIDTH * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return ESP_ERR_NO_MEM;

    for (int i = 0; i < GC9A01_LCD_WIDTH; i++) {
        buf[i] = color;
    }
    for (int y = 0; y < GC9A01_LCD_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(handle->panel, 0, y, GC9A01_LCD_WIDTH, y + 1, buf);
    }

    free(buf);
    return ESP_OK;
}

esp_err_t gc9a01_draw_bitmap(gc9a01_handle_t *handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    return esp_lcd_panel_draw_bitmap(handle->panel, x_start, y_start, x_end, y_end, color_data);
}

uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

esp_err_t gc9a01_set_backlight(bool on)
{
    return sc_led_on(on);
}

// --- device-level LCD (single panel on this board) ---

static gc9a01_handle_t s_lcd;

static struct {
    struct arg_int *r;
    struct arg_int *g;
    struct arg_int *b;
    struct arg_end *end;
} lcd_color_args;

static int cmd_lcd_color(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&lcd_color_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, lcd_color_args.end, argv[0]);
        return 1;
    }

    int r = lcd_color_args.r->ival[0];
    int g = lcd_color_args.g->ival[0];
    int b = lcd_color_args.b->ival[0];

    ESP_LOGI(TAG, "LCD fill RGB(%d,%d,%d)", r, g, b);
    gc9a01_fill_color(&s_lcd, gc9a01_rgb565(r, g, b));
    return 0;
}

/* The panel has no MISO; GC9A01 answers reads on SDA (3-wire read). The SPI
 * device owns the pins, so take them as GPIO, bit-bang at ~100 kHz, then hand
 * the IOMUX pins back to FSPI. A reply proves the controller hears commands. */
#define LCD_BB_HALF_US 5

static void lcd_bb_clock_out(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(GC9A01_PIN_MOSI, (b >> i) & 1);
        esp_rom_delay_us(LCD_BB_HALF_US);
        gpio_set_level(GC9A01_PIN_SCLK, 1);
        esp_rom_delay_us(LCD_BB_HALF_US);
        gpio_set_level(GC9A01_PIN_SCLK, 0);
    }
}

static uint64_t lcd_bb_read(uint8_t cmd, int bits)
{
    gpio_set_direction(GC9A01_PIN_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_level(GC9A01_PIN_DC, 0);
    gpio_set_level(GC9A01_PIN_CS, 0);
    esp_rom_delay_us(LCD_BB_HALF_US);
    lcd_bb_clock_out(cmd);
    gpio_set_direction(GC9A01_PIN_MOSI, GPIO_MODE_INPUT);
    gpio_set_level(GC9A01_PIN_DC, 1);
    uint64_t v = 0;
    for (int i = 0; i < bits; i++) {
        esp_rom_delay_us(LCD_BB_HALF_US);
        gpio_set_level(GC9A01_PIN_SCLK, 1);
        v = (v << 1) | gpio_get_level(GC9A01_PIN_MOSI);
        esp_rom_delay_us(LCD_BB_HALF_US);
        gpio_set_level(GC9A01_PIN_SCLK, 0);
    }
    gpio_set_level(GC9A01_PIN_CS, 1);
    esp_rom_delay_us(LCD_BB_HALF_US * 4);
    return v;
}

static int cmd_lcd_id(int argc, char **argv)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GC9A01_PIN_SCLK) | (1ULL << GC9A01_PIN_CS) | (1ULL << GC9A01_PIN_MOSI),
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    gpio_set_level(GC9A01_PIN_CS, 1);
    gpio_set_level(GC9A01_PIN_SCLK, 0);
    gpio_config(&cfg);

    /* Lengths include the dummy clock some reads start with; print raw bits. */
    static const struct { uint8_t cmd; int bits; const char *name; } reads[] = {
        {0x04, 25, "RDDID (dummy+ID1..3)"},
        {0x09, 33, "RDDST (dummy+status)"},
        {0x0A, 9,  "RDDPM (power mode)"},
        {0xDA, 9,  "RDID1"},
        {0xDB, 9,  "RDID2"},
        {0xDC, 9,  "RDID3"},
    };
    for (int i = 0; i < sizeof(reads) / sizeof(reads[0]); i++) {
        uint64_t v = lcd_bb_read(reads[i].cmd, reads[i].bits);
        printf("%02X %-22s raw=0x%0*llX (%d bits)\n", reads[i].cmd, reads[i].name,
               (reads[i].bits + 3) / 4, (unsigned long long)v, reads[i].bits);
    }

    gpio_set_direction(GC9A01_PIN_MOSI, GPIO_MODE_OUTPUT);
    gpio_iomux_output(GC9A01_PIN_SCLK, SPI2_FUNC_NUM);
    gpio_iomux_output(GC9A01_PIN_MOSI, SPI2_FUNC_NUM);
    gpio_iomux_output(GC9A01_PIN_CS, SPI2_FUNC_NUM);
    return 0;
}

esp_err_t sc_lcd_init(void)
{
    ESP_RETURN_ON_ERROR(gc9a01_init(&s_lcd), TAG, "init failed");

    lcd_color_args.r = arg_int1(NULL, NULL, "<r>", "0-255");
    lcd_color_args.g = arg_int1(NULL, NULL, "<g>", "0-255");
    lcd_color_args.b = arg_int1(NULL, NULL, "<b>", "0-255");
    lcd_color_args.end = arg_end(3);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "lcd_color", .help = "Fill LCD with RGB color",
        .func = &cmd_lcd_color, .argtable = &lcd_color_args,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "lcd_id", .help = "Read panel ID/status over SDA (bit-banged 3-wire read)",
        .func = &cmd_lcd_id,
    }));

    return ESP_OK;
}
