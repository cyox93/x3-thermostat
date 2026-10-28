#pragma once
#include "esp_err.h"

/* Inputs the UI understands (docs/ui.md §2.1): K1 = SHORT/LONG, K2 = ROOM */
typedef enum {
    UI_IN_ENC_CW,
    UI_IN_ENC_CCW,
    UI_IN_SHORT,
    UI_IN_LONG,
    UI_IN_ROOM,
} ui_input_t;

/* Starts the LVGL task and draws the first screen. Call after sc_device_init(). */
esp_err_t ui_init(void);
