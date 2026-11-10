#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_console.h"

#include "main.h"

static int _get_ver_cmd(int argc, char **argv)
{
    printf("version : %s %s\n", SC_MODEL, SC_VERSION);
    return 0;
}

esp_err_t sc_mainloop_init(void)
{
    const esp_console_cmd_t get_ver_cmd = {
        .command = "get_ver",
        .help = "get version",
        .hint = NULL,
        .func = &_get_ver_cmd,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&get_ver_cmd));

    ESP_ERROR_CHECK(sc_switchloop_init());
    /* plcloop before sysloop: sysloop registers for its notes and sets its role */
    ESP_ERROR_CHECK(sc_plcloop_init());
    ESP_ERROR_CHECK(sc_sysloop_init());

    return ESP_OK;
}
