#pragma once

/*
 * Interface to the x3-switch board — the peer on the UART that owns the
 * buttons, button LEDs and relays. Contract: docs/at-protocol.md in the x3-switch repository;
 * keep this API a thin mapping of it. The board holds no policy; callers
 * decide what keys and relays mean.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SC_X3SWITCH_CH_NUM  4   /* channels 1..4 for relays, LEDs and keys */

typedef enum {
    SC_X3SWITCH_LED_WHITE  = 'W',
    SC_X3SWITCH_LED_ORANGE = 'O',
    SC_X3SWITCH_LED_BLINK  = 'T',   /* white ↔ orange at the channel's interval */
} sc_x3switch_led_t;

typedef struct {
    bool relay[SC_X3SWITCH_CH_NUM];
    sc_x3switch_led_t led[SC_X3SWITCH_CH_NUM];
    uint16_t led_interval_ms[SC_X3SWITCH_CH_NUM];
    bool led_enable;
    uint8_t led_brightness;         /* 0~100 % */
    bool key[SC_X3SWITCH_CH_NUM];   /* debounced, true = pressed */
} sc_x3switch_status_t;

typedef enum {
    SC_X3SWITCH_EVENT_KEY_DOWN,
    SC_X3SWITCH_EVENT_KEY_UP,
    SC_X3SWITCH_EVENT_READY,        /* board (re)booted: outputs are back to defaults */
} sc_x3switch_event_t;

/* Unsolicited notifications ("+KEY:", "+READY:"). ch is 1..4 for keys, 0 for
 * READY. Runs in the UART reader task — hand the event off to a loop; calling
 * sc_x3switch_* from here would wait on the reader itself. */
typedef void (*sc_x3switch_callback_t)(sc_x3switch_event_t event, int ch, void *arg);

/* The command functions block until OK/ERROR (a few ms; 100 ms timeout when
 * the board is absent). Safe to call from loop handlers. */
esp_err_t sc_x3switch_init(void);
esp_err_t sc_x3switch_register_callback(sc_x3switch_callback_t callback, void *arg);

esp_err_t sc_x3switch_ping(void);
esp_err_t sc_x3switch_reset(void);
esp_err_t sc_x3switch_version(char *buf, size_t len);
esp_err_t sc_x3switch_status(sc_x3switch_status_t *status);

esp_err_t sc_x3switch_relay_set(int ch, bool on);

esp_err_t sc_x3switch_led_set(int ch, sc_x3switch_led_t mode);
esp_err_t sc_x3switch_led_interval(int ch, uint16_t ms);
esp_err_t sc_x3switch_led_enable(bool on);
esp_err_t sc_x3switch_led_brightness(uint8_t percent);
