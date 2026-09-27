idf_component_register(
    SRCS "switchloop.c" "sysloop.c" "plcloop.c" "mainloop.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES device core x3switch plc esp_timer esp_driver_uart
)
