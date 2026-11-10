#pragma once

/*
 * Boiler-bus controller roles in pure C (docs/plc-protocol.md §5 · §7): the
 * room controller (LT200SPD role, address 2~6) and the living-room controller
 * (DT-200 role, address 1). It turns each received message into the answer to
 * send in our poll slot, and keeps what it overhears of the other devices.
 *
 * No timers, no I/O: the caller feeds complete, checksum-checked messages with
 * the time, sends what comes back, and acts on the change flags. Built for
 * ESP-IDF and for the host tests (tools/sim/tests).
 */

#include <stdbool.h>
#include <stdint.h>

#define SC_PROTO_MSG_MAX        15      /* header length nibble */
#define SC_PROTO_ADDR_MAX       6       /* living room 1 + rooms 2~6 */
#define SC_PROTO_ADDR_LIVING    1
#define SC_PROTO_TEMP_NONE      (-1)    /* ×2 values below */

/* Mode values (items 00 / 08) */
#define SC_PROTO_MODE_HEAT      0x20
#define SC_PROTO_MODE_RESERVE   0x28
#define SC_PROTO_MODE_OFF       0x2A
#define SC_PROTO_MODE_AWAY      0x2B
#define SC_PROTO_MODE_TIMER     0x40

typedef struct {
    uint8_t len;                        /* 0 = nothing */
    uint8_t b[SC_PROTO_MSG_MAX];
} sc_proto_msg_t;

/* What we know about one address, ours or overheard. Temperatures are ×2
 * (0.5 °C steps) as on the bus; SC_PROTO_TEMP_NONE until seen. */
typedef struct {
    bool seen;
    bool demand;                        /* 0x20 in its last message */
    int16_t temp2;                      /* room temperature */
    int16_t set2;                       /* heating setpoint */
    int16_t mode;                       /* -1 until seen */
} sc_proto_room_t;

/* Change flags for the caller */
typedef enum {
    SC_PROTO_CHG_OWN     = 1 << 0,      /* our mode / setpoint set over the bus (A5) */
    SC_PROTO_CHG_ROOMS   = 1 << 1,      /* an overheard room changed: see rooms_changed */
    SC_PROTO_CHG_BOILER  = 1 << 2,      /* boiler bit changed */
    SC_PROTO_CHG_DEMAND  = 1 << 3,      /* our demand bit changed */
} sc_proto_chg_t;

typedef struct {
    sc_proto_msg_t tx;                  /* send now, in our slot */
    uint32_t chg;                       /* sc_proto_chg_t */
    uint8_t rooms_changed;              /* bit per address */
} sc_proto_out_t;

#define SC_PROTO_NOTIFY_MAX     4
#define SC_PROTO_REQUEST_MAX    8

typedef struct {
    uint8_t addr;                       /* ours; 0 = listen only */

    /* Our controller */
    uint8_t mode;
    uint8_t set2;
    uint8_t away2;                      /* away temperature (10 °C) */
    int16_t temp2;                      /* SC_PROTO_TEMP_NONE until the sensor reads */
    bool demand;
    bool registered;                    /* the MC200 polls us with 0x40 */
    bool next_temp;                     /* E4 alternates hour counter / temperature */

    /* Hour counter (E4 kind 0): started from the first value heard, then
       counted here — each device counts on its own clock (§5.8) */
    uint8_t hour;
    bool hour_heard;
    int64_t hour_ms;                    /* when it last changed */

    /* Changes to report with C5, oldest first; the head repeats until B3 */
    struct { uint8_t item, value; } notify[SC_PROTO_NOTIFY_MAX];
    uint8_t n_notify;

    /* Living room only: requests sent in our slot (F5 to rooms, D4 reads) */
    sc_proto_msg_t request[SC_PROTO_REQUEST_MAX];
    uint8_t n_request;
    bool connected;                     /* D4 01 1E sent after registering */
    int64_t hot_water_ms;               /* last D4 01 3B */

    /* Overheard */
    sc_proto_room_t room[SC_PROTO_ADDR_MAX + 1];
    uint8_t read_item[SC_PROTO_ADDR_MAX + 1];   /* last 94 item per address */
    bool boiler;
    int16_t hot_water2;                 /* MC200 item 3B, SC_PROTO_TEMP_NONE until read */
} sc_proto_t;

/* addr 0 = listen only; 1 = living room (DT-200 role); 2~6 = room */
void sc_proto_init(sc_proto_t *p, uint8_t addr, uint8_t mode, uint8_t set2);

/* One complete message whose checksum passed, not our own echo */
void sc_proto_rx(sc_proto_t *p, const uint8_t *m, int len, int64_t now_ms, sc_proto_out_t *out);

/* Local changes (UI). They are reported to the MC200 with C5. */
void sc_proto_set_mode(sc_proto_t *p, uint8_t mode, sc_proto_out_t *out);
void sc_proto_set_setpoint(sc_proto_t *p, uint8_t set2, sc_proto_out_t *out);
/* Room temperature from our sensor, ×2; SC_PROTO_TEMP_NONE when lost */
void sc_proto_set_temp(sc_proto_t *p, int16_t temp2, sc_proto_out_t *out);

/* Living room only: set another room through the MC200 (F5 → A5) */
bool sc_proto_request_set(sc_proto_t *p, uint8_t addr, uint8_t item, uint8_t value);

/* Mode on the bus meaning "heating on" for the UI */
static inline bool sc_proto_mode_is_heat(int mode) { return mode == SC_PROTO_MODE_HEAT || mode == SC_PROTO_MODE_TIMER; }

/* Checksum = sum of the preceding bytes & 0x7F; fills header length and sum */
void sc_proto_seal(sc_proto_msg_t *m);
