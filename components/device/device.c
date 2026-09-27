#include "esp_log.h"
#include "device.h"

static const char *TAG = "device";

esp_err_t sc_device_init(void)
{
    ESP_LOGI(TAG, "Device init");

    ESP_ERROR_CHECK(sc_led_init());
    sc_led_brightness(255);
    ESP_ERROR_CHECK(sc_lcd_init());
    ESP_ERROR_CHECK(sc_encoder_init());
    ESP_ERROR_CHECK(sc_gpio_cmd_init());
    ESP_ERROR_CHECK(sc_als_init());
    ESP_ERROR_CHECK(sc_temp_humid_init());

    return ESP_OK;
}
