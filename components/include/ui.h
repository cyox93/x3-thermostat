#pragma once
#include "esp_err.h"

/* Starts the LVGL task and draws the first screen. Call after sc_device_init(). */
esp_err_t ui_init(void);
