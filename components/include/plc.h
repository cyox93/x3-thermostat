#pragma once

/*
 * PLC bus link through the dt200-plc-module (T6B70BFG, 500 kHz ASK on the
 * 12 V two-wire boiler bus). This is the frame layer only: one frame carries
 * one byte. What the bytes mean is up to the caller (docs/plc-protocol.md).
 *
 * The first bit on the wire is the LSB — with that order the checksum and
 * room numbers come out plain (docs/plc-protocol.md §5).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Max bytes (= frames) per sc_plc_send */
#define SC_PLC_SEND_MAX     8

/* Set in the RX value for our own carrier seen back through the module: a
 * burst that closes during a send or just after it (it closes only after the
 * RX idle time, so the echo of a send decodes once the send has finished). */
#define SC_PLC_RX_ECHO      0x100

typedef enum {
    SC_PLC_EVENT_RX,            /* value = byte (| SC_PLC_RX_ECHO) */
    SC_PLC_EVENT_RX_ERROR,      /* value = sc_plc_rx_error_t */
    SC_PLC_EVENT_TX_DONE,       /* value = 0 */
} sc_plc_event_t;

typedef enum {
    SC_PLC_RX_ERR_NO_START = 1, /* burst without a START pulse (noise) */
    SC_PLC_RX_ERR_BITS,         /* START seen but not exactly 8 data bits */
} sc_plc_rx_error_t;

/* Runs in ISR context — only hand the value off (sc_eventloop_post_event_from_isr). */
typedef void (*sc_plc_callback_t)(sc_plc_event_t event, int value, void *arg);

esp_err_t sc_plc_init(void);
esp_err_t sc_plc_register_callback(sc_plc_callback_t callback, void *arg);

/* Sends one frame per byte, back to back at the bus frame period. Returns at
 * once; SC_PLC_EVENT_TX_DONE follows. ESP_ERR_INVALID_STATE while a send is
 * still in progress. */
esp_err_t sc_plc_send(const uint8_t *data, size_t len);
bool sc_plc_tx_busy(void);

/* The last burst that did not decode, for finding out why: each entry is
 * level << 15 | duration in µs, in order. Overwritten by the next bad burst. */
#define SC_PLC_BURST_LEVEL  0x8000
#define SC_PLC_BURST_MAX    2048
size_t sc_plc_get_error_burst(uint16_t *buf, size_t max);

/* Carrier on the bus right now (DOUT_BUF level) */
bool sc_plc_carrier(void);
