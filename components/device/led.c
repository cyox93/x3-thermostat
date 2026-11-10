#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "board.h"
#include "device.h"

#define LED_GPIO_ON    BOARD_BL_RING_PIN
#define LED_GPIO_COLOR BOARD_RING_COLOR_PIN

#define LED_LEDC_TIMER   LEDC_TIMER_0
#define LED_LEDC_CHANNEL LEDC_CHANNEL_0
#define LED_LEDC_FREQ    5000

static const char *TAG = "led";
static uint8_t s_last_brightness = 255;

esp_err_t sc_led_on(bool on)
{
    uint8_t duty = on ? s_last_brightness : 0;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_LEDC_CHANNEL, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_LEDC_CHANNEL), TAG, "update duty failed");
    ESP_LOGI(TAG, "LED %s (brightness %d)", on ? "on" : "off", duty);
    return ESP_OK;
}

esp_err_t sc_led_brightness(uint8_t level)
{
    s_last_brightness = level ? level : 1;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_LEDC_CHANNEL, level), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_LEDC_CHANNEL), TAG, "update duty failed");
    ESP_LOGD(TAG, "LED brightness %d", level);      /* every step of a UI fade */
    return ESP_OK;
}

esp_err_t sc_led_color(int color)
{
    gpio_set_level(LED_GPIO_COLOR, color ? 1 : 0);
    ESP_LOGI(TAG, "LED color %s", color ? "white" : "red");
    return ESP_OK;
}

// --- console commands ---

static struct { struct arg_str *state; struct arg_end *end; } led_on_args;

static int cmd_led_on(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&led_on_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, led_on_args.end, argv[0]);
        return 1;
    }
    const char *val = led_on_args.state->sval[0];
    if (strcmp(val, "on") == 0) {
        sc_led_on(true);
    } else if (strcmp(val, "off") == 0) {
        sc_led_on(false);
    } else {
        printf("Invalid: '%s' (use on, off)\n", val);
        return 1;
    }
    return 0;
}

static struct { struct arg_int *level; struct arg_end *end; } led_brightness_args;

static int cmd_led_brightness(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&led_brightness_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, led_brightness_args.end, argv[0]);
        return 1;
    }
    int level = led_brightness_args.level->ival[0];
    if (level < 0 || level > 255) {
        printf("Invalid: %d (use 0-255)\n", level);
        return 1;
    }
    sc_led_brightness(level);
    printf("LED brightness %d\n", level);
    return 0;
}

static struct { struct arg_str *color; struct arg_end *end; } led_color_args;

static int cmd_led_color(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&led_color_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, led_color_args.end, argv[0]);
        return 1;
    }
    const char *val = led_color_args.color->sval[0];
    if (strcmp(val, "red") == 0) {
        sc_led_color(SC_LED_RED);
    } else if (strcmp(val, "white") == 0) {
        sc_led_color(SC_LED_WHITE);
    } else {
        printf("Invalid: '%s' (use red, white)\n", val);
        return 1;
    }
    return 0;
}

esp_err_t sc_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO_COLOR,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(LED_GPIO_COLOR, 0);

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LED_LEDC_TIMER,
        .freq_hz = LED_LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = LED_GPIO_ON,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LED_LEDC_CHANNEL,
        .timer_sel = LED_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    led_on_args.state = arg_str1(NULL, NULL, "<state>", "on, off");
    led_on_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "led_on", .help = "LED on/off (restores last brightness)",
        .func = &cmd_led_on, .argtable = &led_on_args,
    }));

    led_brightness_args.level = arg_int1(NULL, NULL, "<0-255>", "brightness level");
    led_brightness_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "led_brightness", .help = "Set LED brightness (0-255)",
        .func = &cmd_led_brightness, .argtable = &led_brightness_args,
    }));

    led_color_args.color = arg_str1(NULL, NULL, "<color>", "red, white");
    led_color_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "led_color", .help = "Set LED color (red, white)",
        .func = &cmd_led_color, .argtable = &led_color_args,
    }));

    ESP_LOGI(TAG, "LED initialized (brightness=GPIO%d PWM, color=GPIO%d)", LED_GPIO_ON, LED_GPIO_COLOR);
    return ESP_OK;
}
