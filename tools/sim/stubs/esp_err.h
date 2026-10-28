#pragma once
/* ui.h includes esp_err.h; the sim never calls ui_init(), so a type is enough */
typedef int esp_err_t;
#define ESP_OK 0
