#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "board.h"
#include "device.h"

#define ENCODER_GPIO_A  BOARD_ENC_A_PIN
#define ENCODER_GPIO_B  BOARD_ENC_B_PIN
/* Counts per detent. The old polling loop treated a change of 2 as one step;
 * re-check on the main-board (docs/roadmap.md Phase A). */
#define ENCODER_COUNTS_PER_DETENT  2
#define ENCODER_GLITCH_NS       1000

static const char *TAG = "encoder";
static pcnt_unit_handle_t s_pcnt_unit;
static sc_encoder_callback_t s_callback;
static void *s_callback_arg;

/* The limits double as watch points: every detent the counter hits one,
 * the ISR fires once and the unit wraps to 0. accum_count keeps the running
 * total for encoder_read. */
static bool on_detent(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *arg)
{
    if (s_callback) {
        s_callback(edata->watch_point_value > 0 ? 1 : -1, s_callback_arg);
    }
    return false;
}

esp_err_t sc_encoder_register_callback(sc_encoder_callback_t callback, void *arg)
{
    s_callback_arg = arg;
    s_callback = callback;
    return ESP_OK;
}

esp_err_t sc_encoder_get_count(int *value)
{
    return pcnt_unit_get_count(s_pcnt_unit, value);
}

esp_err_t sc_encoder_clear(void)
{
    return pcnt_unit_clear_count(s_pcnt_unit);
}

static int cmd_encoder_read(int argc, char **argv)
{
    int count = 0;
    sc_encoder_get_count(&count);
    printf("encoder: %d\n", count);
    return 0;
}

static int cmd_encoder_clear(int argc, char **argv)
{
    sc_encoder_clear();
    printf("encoder: cleared\n");
    return 0;
}

esp_err_t sc_encoder_init(void)
{
    pcnt_unit_config_t unit_cfg = {
        .high_limit = ENCODER_COUNTS_PER_DETENT,
        .low_limit = -ENCODER_COUNTS_PER_DETENT,
        .flags.accum_count = 1,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &s_pcnt_unit), TAG, "new unit failed");

    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = ENCODER_GLITCH_NS,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_cfg), TAG, "set filter failed");

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = ENCODER_GPIO_A,
        .level_gpio_num = ENCODER_GPIO_B,
    };
    pcnt_channel_handle_t chan_a;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_a_cfg, &chan_a), TAG, "new chan_a failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE), TAG, "set edge_a failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "set level_a failed");

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = ENCODER_GPIO_B,
        .level_gpio_num = ENCODER_GPIO_A,
    };
    pcnt_channel_handle_t chan_b;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_b_cfg, &chan_b), TAG, "new chan_b failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE), TAG, "set edge_b failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "set level_b failed");

    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, ENCODER_COUNTS_PER_DETENT), TAG, "watch high failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, -ENCODER_COUNTS_PER_DETENT), TAG, "watch low failed");
    pcnt_event_callbacks_t cbs = {
        .on_reach = on_detent,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_register_event_callbacks(s_pcnt_unit, &cbs, NULL), TAG, "register callback failed");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit), TAG, "enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "clear failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit), TAG, "start failed");

    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "encoder_read", .help = "Read encoder count",
        .func = &cmd_encoder_read,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "encoder_clear", .help = "Clear encoder count",
        .func = &cmd_encoder_clear,
    }));

    ESP_LOGI(TAG, "Encoder initialized (A=GPIO%d, B=GPIO%d)", ENCODER_GPIO_A, ENCODER_GPIO_B);
    return ESP_OK;
}
