/*
 * Ambient light — ALS-PT19-315C phototransistor into a load resistor, read by
 * ADC1. Datasheet (Everlight DLS-0000039 Rev.8): photocurrent is linear, typ
 * 0.15 µA/lux under white fluorescent/LED light (IPH2 150 µA @ 1000 lx), but
 * min 0.05 µA/lux and ~3.5x higher under incandescent light. The lux value is
 * therefore approximate — good enough for brightness steps, not a lux meter.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "board.h"
#include "device.h"

/* The sensor output never exceeds the 3V3 supply, and 12 dB attenuation
 * covers ~0~3.1 V, so the full swing across the load resistor is readable. */
#define ALS_ADC_ATTEN    ADC_ATTEN_DB_12
/* One sample per tick (1 ms) across 10 ms spans a full 100 Hz / 120 Hz
 * flicker period of mains-powered lights, so the average is not a snapshot
 * of the bright or dark half. Yields between samples. */
#define ALS_SAMPLES      10
#define ALS_SAMPLE_TICKS 1

/* Datasheet typ, white fluorescent/LED (Note 2 light source) */
#define ALS_UA_PER_LUX   0.15f
/* Output tops out at VCC - VCE(sat) = 3.3 - 0.4 V; above this, lux is a floor */
#define ALS_SAT_MV       2850

static const char *TAG = "als";
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_chan;
static adc_cali_handle_t s_cali;

esp_err_t sc_als_read_mv(int *mv)
{
    int sum = 0;
    for (int i = 0; i < ALS_SAMPLES; i++) {
        int raw = 0;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc, s_chan, &raw), TAG, "read failed");
        sum += raw;
        if (i + 1 < ALS_SAMPLES) {
            vTaskDelay(ALS_SAMPLE_TICKS);
        }
    }
    int raw = sum / ALS_SAMPLES;

    if (s_cali) {
        return adc_cali_raw_to_voltage(s_cali, raw, mv);
    }
    /* Uncalibrated fallback — only good enough for relative brightness. */
    *mv = raw * 3100 / 4095;
    return ESP_OK;
}

static float _mv_to_lux(int mv)
{
    float ua = mv * 1000.0f / BOARD_ALS_LOAD_OHM;     /* mV / Ω = mA → µA */
    return ua / ALS_UA_PER_LUX;
}

esp_err_t sc_als_read_lux(float *lux)
{
    int mv;
    ESP_RETURN_ON_ERROR(sc_als_read_mv(&mv), TAG, "read failed");

    /* Past saturation the reading stops rising; report the ceiling. */
    *lux = _mv_to_lux(mv < ALS_SAT_MV ? mv : ALS_SAT_MV);
    return ESP_OK;
}

static int cmd_als_read(int argc, char **argv)
{
    int mv = 0;
    if (sc_als_read_mv(&mv) != ESP_OK) {
        printf("als: read failed\n");
        return 1;
    }
    printf("als: %d mV%s, ~%.0f lux%s\n",
           mv, s_cali ? "" : " (ADC uncalibrated)",
           _mv_to_lux(mv), mv >= ALS_SAT_MV ? " (saturated)" : "");
    return 0;
}

esp_err_t sc_als_init(void)
{
    adc_unit_t unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(BOARD_ALS_PIN, &unit, &s_chan), TAG, "GPIO%d is not an ADC pin", BOARD_ALS_PIN);
    ESP_RETURN_ON_FALSE(unit == ADC_UNIT_1, ESP_ERR_INVALID_ARG, TAG, "GPIO%d is on ADC2, which WiFi blocks", BOARD_ALS_PIN);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = unit,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc), TAG, "new unit failed");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ALS_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_chan, &chan_cfg), TAG, "config channel failed");

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .chan = s_chan,
        .atten = ALS_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "no eFuse calibration, using raw scale");
        s_cali = NULL;
    }

    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "als_read", .help = "Read ambient light (mV and approximate lux)",
        .func = &cmd_als_read,
    }));


    ESP_LOGI(TAG, "ALS initialized (GPIO%d, ADC1_CH%d)", BOARD_ALS_PIN, s_chan);
    return ESP_OK;
}
