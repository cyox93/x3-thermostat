#pragma once
#include <stdbool.h>
#include <stdint.h>
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

/* Any thread: queues the input for the LVGL task and returns (docs/ui.md §3.3).
   Dropped with a warning when the queue is full. */
void ui_input(ui_input_t in);

#define UI_TEMP_NONE INT16_MIN
#define UI_HUM_NONE  (-1)

/* Any thread: this unit's own sensor, shown in the installed room (docs/ui.md
   §3.3). UI_TEMP_NONE / UI_HUM_NONE clear a value the sensor no longer gives. */
void ui_local_climate_update(int16_t temp_x10, int8_t humidity);

/* Called in the LVGL task with the installed room (app_room_t) and whether
   heating is on in the room on screen (the one K1 switches): once soon after
   registering, then whenever either changes. The receiver only posts. */
typedef void (*ui_room_callback_t)(int room, bool heat_on, void *arg);
void ui_register_room_callback(ui_room_callback_t cb, void *arg);
