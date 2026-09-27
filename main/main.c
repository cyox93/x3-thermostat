#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"

#include "config.h"
#include "console.h"
#include "device.h"
#include "x3switch.h"
#include "plc.h"
#include "main.h"

void app_main(void)
{
    printf("\n\n%s %s\n\n", SC_MODEL, SC_VERSION);

    ESP_ERROR_CHECK(sc_config_init());
    ESP_ERROR_CHECK(sc_console_init());
    ESP_ERROR_CHECK(sc_device_init());
    ESP_ERROR_CHECK(sc_x3switch_init());
    ESP_ERROR_CHECK(sc_plc_init());

    ESP_ERROR_CHECK(sc_mainloop_init());
}
