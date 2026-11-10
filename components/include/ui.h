#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Inputs the UI understands (docs/ui.md §2.1): K1 = SHORT/LONG, K2 = ROOM.
   Light keys never reach the UI, so they do not wake the screen. */
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

/* Any thread: the room is dark (sysloop's ambient level, docs/architecture.md
   §3.5). While dark the LCD runs at 10 % whatever the brightness setting. */
void ui_ambient_dark(bool dark);

/* Called in the LVGL task with the installed room (app_room_t) and whether
   heating is on in the room on screen (the one K1 switches): once soon after
   registering, then whenever either changes. The receiver only posts. */
typedef void (*ui_room_callback_t)(int room, bool heat_on, void *arg);
void ui_register_room_callback(ui_room_callback_t cb, void *arg);

/* Any thread: another room's values (the living-room unit, from the boiler
   bus). Rooms are app_room_t. */
void ui_room_climate_update(int room, int16_t temp_x10, int8_t humidity);
void ui_room_burn_update(int room, bool burning);
/* on/target changed outside the UI (bus). Ignored for a room whose own change
   from the knob is still waiting to be reported. Never reported back. */
void ui_room_heat_update(int room, bool on, int16_t target_x10);

/* Called in the LVGL task when the user changed a room's on/target, once the
   knob has been still for 300 ms (docs/ui.md §3.3) — and once per room at
   start with the values the UI begins with. The receiver only posts. */
typedef void (*ui_heat_callback_t)(int room, bool on, int16_t target_x10, void *arg);
void ui_register_heat_callback(ui_heat_callback_t cb, void *arg);
