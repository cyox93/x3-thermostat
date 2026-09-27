idf_component_register(
    SRCS "encoder.c" "gpio_cmd.c" "device.c" "gc9a01.c" "led.c" "als.c" "temp_humid.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES driver esp_lcd console esp_driver_ledc esp_driver_pcnt esp_adc esp_driver_i2c
)
