idf_component_register(
    SRCS "x3switch.c" "x3switch_cmd.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_driver_uart esp_driver_gpio console
)
