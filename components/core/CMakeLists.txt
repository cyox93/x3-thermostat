set(extra_requires "cmd_nvs" "cmd_system")

idf_component_register(
    SRCS "eventloop.c" "console.c" "config.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_event esp_timer freertos console nvs_flash driver vfs esp_driver_uart esp_usb_cdc_rom_console esp_driver_usb_serial_jtag ${extra_requires}
)
