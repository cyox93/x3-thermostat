#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "x3switch.h"
#include "plc.h"
#include "plc_proto.h"

#define SC_MODEL    "X3THERMOSTAT"
#define SC_VERSION  "0.1.0"

esp_err_t sc_mainloop_init(void);

/*
 * switchloop — owner of the x3-switch board: relays, button LEDs and keys.
 *
 * The setters only record the wanted state and return; switchloop sends the
 * AT commands from its own thread, and pushes everything again when the
 * board reports it has rebooted. Callers never wait on the UART.
 */
typedef enum {
    SC_SWITCH_KEY_CLICK,        /* released before the long-press time */
    SC_SWITCH_KEY_LONG,         /* held for the long-press time (fires while held) */
} sc_switch_key_t;

/* Runs in the switchloop thread. ch = 1..4 (K1..K4). */
typedef void (*sc_switch_key_callback_t)(int ch, sc_switch_key_t event, void *arg);

esp_err_t sc_switchloop_init(void);
void sc_switchloop_end(void);
esp_err_t sc_switchloop_register_key_callback(sc_switch_key_callback_t callback, void *arg);
/* Runs in the switchloop thread, once, after the board's outputs have been
   adopted at boot — sc_switch_relay_get() then reflects the board. */
typedef void (*sc_switch_ready_callback_t)(void *arg);
esp_err_t sc_switchloop_register_ready_callback(sc_switch_ready_callback_t callback, void *arg);
bool sc_switch_is_ready(void);
/* The wanted relay state (what switchloop drives the board to) */
bool sc_switch_relay_get(int ch);

esp_err_t sc_switch_relay_set(int ch, bool on);
esp_err_t sc_switch_led_set(int ch, sc_x3switch_led_t mode);
esp_err_t sc_switch_led_interval(int ch, uint16_t ms);
esp_err_t sc_switch_led_enable(bool on);
esp_err_t sc_switch_led_brightness(uint8_t percent);

/*
 * sysloop — owner of the unit's policy state. Keys (via switchloop), the
 * encoder and periodic sensor timers all land on its eventloop, so every
 * decision happens in one thread.
 */
typedef struct sc_sysinfo {
    float temp;             /* °C, valid when climate_valid */
    float humid;            /* %RH */
    bool climate_valid;

    float ambient_lux;      /* approximate (datasheet typical sensitivity) */
    int bright_level;       /* sc_bright_level_t */
    bool screen_on;
} sc_sysinfo_t;

/* Display / button LED brightness, chosen from ambient light */
typedef enum {
    SC_BRIGHT_NIGHT,        /* lights off, sleeping — screen off unless touched */
    SC_BRIGHT_DIM,          /* lamp / indirect light */
    SC_BRIGHT_NORMAL,       /* room lights on */
} sc_bright_level_t;

esp_err_t sc_sysloop_init(void);
void sc_sysloop_end(void);
const sc_sysinfo_t *sc_sysloop_get_info(void);

/*
 * plcloop — owner of the PLC bus link to the boiler controller. Frames are
 * gathered into groups (frames 12 ms apart, groups 254 ms apart) and into
 * messages for the controller role (components/plcproto, docs/plc-protocol.md
 * §7): a room controller at address 2~6, or the living-room controller at 1.
 *
 * Once a role is set, its answers go on the bus: no original controller may
 * remain at that address.
 */
esp_err_t sc_plcloop_init(void);
void sc_plcloop_end(void);
/* Queues one frame per byte (≤ SC_PLC_SEND_MAX); the plcloop thread sends them */
esp_err_t sc_plcloop_send(const uint8_t *data, size_t len);

/* Our role: address 1 (living room) or 2~6 (room), 0 = listen only, with the
   controller's mode and setpoint (×2, 0.5 °C) to start from */
esp_err_t sc_plcloop_set_role(uint8_t addr, uint8_t mode, uint8_t set2);
/* Local changes (UI), reported to the MC200 with C5 */
esp_err_t sc_plcloop_set_heat(uint8_t mode, uint8_t set2);
/* Room temperature ×2 from our sensor; SC_PROTO_TEMP_NONE when lost */
esp_err_t sc_plcloop_set_temp(int16_t temp2);
/* Living room only: set another room through the MC200 (F5) */
esp_err_t sc_plcloop_request_set(uint8_t addr, uint8_t item, uint8_t value);

/* What the bus told us. Runs in the plcloop thread: post, don't act. */
typedef enum {
    SC_PLC_NOTE_OWN,        /* our mode / setpoint set over the bus (A5) */
    SC_PLC_NOTE_ROOM,       /* an overheard room changed */
    SC_PLC_NOTE_STATE,      /* boiler bit or our demand changed */
} sc_plc_note_kind_t;

typedef struct sc_plc_note {
    sc_plc_note_kind_t kind;
    uint8_t addr;           /* ROOM: whose */
    bool demand;            /* ROOM: theirs; OWN / STATE: ours */
    bool boiler;
    int16_t temp2;          /* ROOM: ×2, SC_PROTO_TEMP_NONE unknown */
    int16_t set2;           /* ×2, SC_PROTO_TEMP_NONE unknown */
    int16_t mode;           /* -1 unknown */
} sc_plc_note_t;

typedef void (*sc_plcloop_callback_t)(const sc_plc_note_t *note, void *arg);
esp_err_t sc_plcloop_register_callback(sc_plcloop_callback_t callback, void *arg);
