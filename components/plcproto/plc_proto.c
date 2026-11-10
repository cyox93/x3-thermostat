/* Boiler-bus controller roles — docs/plc-protocol.md §5 (messages) and §7
   (what a room / living-room controller must do). Pure C: see plc_proto.h. */
#include <string.h>
#include "plc_proto.h"

/* Address byte (§5.4) */
#define _ADDR_MASK          0x0F
#define _ADDR_REGISTERED    0x40    /* MC200 → controller */
#define _ADDR_DEMAND        0x20    /* controller → MC200 */
#define _ADDR_BOILER        0x10    /* MC200 → anyone */
#define _ADDR_E4_TEMP       0x10    /* E4 value kind: room temperature */

/* MC200 items read by the DT-200 with D4 (§5.5) start here */
#define _MC200_ITEM_MIN     0x30
#define _MC200_HOT_WATER    0x3B

#define _ITEM_MODE          0x00
#define _ITEM_SET           0x01
#define _ITEM_MODE_ALT      0x08    /* same values as 00 (OFF · reserve · remote · all) */
#define _ITEM_FULL          0x1E
#define _NO_READ            0xFF

#define _HOUR_MS            (60LL * 60 * 1000)
#define _HOT_WATER_MS       5000    /* the DT-200 reads item 3B about every 5 s */
#define _FROST2             10      /* 5 °C: heat regardless of mode (manual) */
#define _AWAY2_DEFAULT      20      /* 10 °C (manual) */

void sc_proto_seal(sc_proto_msg_t *m)
{
    uint32_t sum = 0;
    m->b[0] = (m->b[0] & 0xF0) | m->len;
    for (int i = 0; i < m->len - 1; i++) sum += m->b[i];
    m->b[m->len - 1] = sum & 0x7F;
}

static void _msg(sc_proto_msg_t *m, int len, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    const uint8_t v[5] = { b0, b1, b2, b3, b4 };
    m->len = len;
    for (int i = 0; i < len - 1 && i < 5; i++) m->b[i] = v[i];
    sc_proto_seal(m);
}

static bool _living(const sc_proto_t *p) { return p->addr == SC_PROTO_ADDR_LIVING; }

static uint8_t _own_addr_byte(const sc_proto_t *p)
{
    return p->addr | (p->demand ? _ADDR_DEMAND : 0);
}

/* §7 6: heat by mode, and frost protection at 5 °C whatever the mode */
static void _update_demand(sc_proto_t *p, sc_proto_out_t *out)
{
    bool d = false;
    if (p->temp2 != SC_PROTO_TEMP_NONE) {
        if (p->temp2 <= _FROST2) d = true;
        else if (sc_proto_mode_is_heat(p->mode)) d = p->set2 > p->temp2;
        else if (p->mode == SC_PROTO_MODE_AWAY) d = p->away2 > p->temp2;
    }
    if (d != p->demand) {
        p->demand = d;
        if (out) out->chg |= SC_PROTO_CHG_DEMAND;
    }
}

/* Newest value wins for an item already waiting */
static void _notify(sc_proto_t *p, uint8_t item, uint8_t value)
{
    for (int i = 0; i < p->n_notify; i++) {
        if (p->notify[i].item == item) {
            p->notify[i].value = value;
            return;
        }
    }
    if (p->n_notify == SC_PROTO_NOTIFY_MAX) {
        memmove(&p->notify[0], &p->notify[1], sizeof(p->notify[0]) * (SC_PROTO_NOTIFY_MAX - 1));
        p->n_notify--;
    }
    p->notify[p->n_notify].item = item;
    p->notify[p->n_notify].value = value;
    p->n_notify++;
}

static void _hour_tick(sc_proto_t *p, int64_t now_ms)
{
    while (now_ms - p->hour_ms >= _HOUR_MS) {
        p->hour = (p->hour + 1) & 0x1F;
        p->hour_ms += _HOUR_MS;
    }
}

void sc_proto_init(sc_proto_t *p, uint8_t addr, uint8_t mode, uint8_t set2)
{
    memset(p, 0, sizeof(*p));
    p->addr = addr <= SC_PROTO_ADDR_MAX ? addr : 0;
    p->mode = mode;
    p->set2 = set2;
    p->away2 = _AWAY2_DEFAULT;
    p->temp2 = SC_PROTO_TEMP_NONE;
    p->hot_water2 = SC_PROTO_TEMP_NONE;
    p->hot_water_ms = -_HOT_WATER_MS;          /* first read right after connecting */
    for (int a = 0; a <= SC_PROTO_ADDR_MAX; a++) {
        p->room[a] = (sc_proto_room_t){ .temp2 = SC_PROTO_TEMP_NONE, .set2 = SC_PROTO_TEMP_NONE, .mode = -1 };
        p->read_item[a] = _NO_READ;
    }
}

/* ===== Answers in our slot ===== */

/* CA <addr> 1E <mode> <set> <room> 05 <away> 01 — registration and read 1E.
   The 05 and 01 are copied from the LT200SPD (meaning unknown, §8 #6). */
static void _full_state(const sc_proto_t *p, sc_proto_msg_t *m)
{
    m->len = 10;
    m->b[0] = 0xC0;
    m->b[1] = _own_addr_byte(p);
    m->b[2] = _ITEM_FULL;
    m->b[3] = p->mode;
    m->b[4] = p->set2;
    m->b[5] = p->temp2 != SC_PROTO_TEMP_NONE ? (uint8_t)p->temp2 : p->set2;
    m->b[6] = 0x05;
    m->b[7] = p->away2;
    m->b[8] = 0x01;
    sc_proto_seal(m);
}

/* E4 alternates the hour counter and the room temperature (§5.3); without a
   temperature it keeps to the counter */
static void _e4(sc_proto_t *p, int64_t now_ms, sc_proto_msg_t *m)
{
    _hour_tick(p, now_ms);
    bool temp = p->next_temp && p->temp2 != SC_PROTO_TEMP_NONE;
    p->next_temp = !p->next_temp;
    _msg(m, 4, 0xE0, _own_addr_byte(p) | (temp ? _ADDR_E4_TEMP : 0), temp ? (uint8_t)p->temp2 : p->hour, 0, 0);
}

static void _answer_poll(sc_proto_t *p, int64_t now_ms, sc_proto_out_t *out)
{
    sc_proto_msg_t *m = &out->tx;

    if (!p->registered) {                       /* scan poll: register (§5.6) */
        _full_state(p, m);
        return;
    }
    if (p->n_notify) {                          /* repeats until B3 */
        _msg(m, 5, 0xC0, _own_addr_byte(p), p->notify[0].item, p->notify[0].value, 0);
        return;
    }
    if (_living(p)) {
        if (!p->connected) {                    /* have the MC200 collect the rooms (§5.6) */
            p->connected = true;
            _msg(m, 4, 0xD0, _own_addr_byte(p), _ITEM_FULL, 0, 0);
            return;
        }
        if (p->n_request) {
            *m = p->request[0];
            memmove(&p->request[0], &p->request[1], sizeof(p->request[0]) * (p->n_request - 1));
            p->n_request--;
            return;
        }
        if (now_ms - p->hot_water_ms >= _HOT_WATER_MS) {
            p->hot_water_ms = now_ms;
            _msg(m, 4, 0xD0, _own_addr_byte(p), _MC200_HOT_WATER, 0, 0);
            return;
        }
    }
    _e4(p, now_ms, m);
}

/* 94 read: answers in the read numbering (§5.5) */
static void _answer_read(sc_proto_t *p, uint8_t item, int64_t now_ms, sc_proto_out_t *out)
{
    sc_proto_msg_t *m = &out->tx;
    switch (item) {
    case 0x00: _msg(m, 5, 0xC0, _own_addr_byte(p), 0x00, p->mode, 0); break;
    case 0x02: _msg(m, 6, 0xC0, _own_addr_byte(p), 0x02, p->mode, p->set2); break;
    case 0x04: _msg(m, 6, 0xC0, _own_addr_byte(p), 0x04, p->mode, 0x00); break;
    case 0x06: _msg(m, 5, 0xC0, _own_addr_byte(p), 0x06, p->away2, 0); break;
    case _ITEM_FULL: _full_state(p, m); break;
    default: _e4(p, now_ms, m); break;          /* unknown item: a plain answer */
    }
}

/* A5 for us: a setting from the DT-200, wallpad or app (§5.6). Not echoed
   back with C5. A mode without 0x20 on item 08 ends an all-off: back to it. */
static void _apply(sc_proto_t *p, uint8_t item, uint8_t value, sc_proto_out_t *out)
{
    switch (item) {
    case _ITEM_MODE_ALT:
        if (!(value & 0x20) && value != SC_PROTO_MODE_TIMER) value |= 0x20;
        /* fall through */
    case _ITEM_MODE:
        if (p->mode != value) { p->mode = value; out->chg |= SC_PROTO_CHG_OWN; }
        break;
    case _ITEM_SET:
        if (p->set2 != value) { p->set2 = value; out->chg |= SC_PROTO_CHG_OWN; }
        break;
    default:                                    /* timer · reserve · hot water: not kept */
        break;
    }
    _update_demand(p, out);
}

/* ===== Overheard ===== */

static void _room_set(sc_proto_t *p, int a, int16_t *field, int16_t v, sc_proto_out_t *out)
{
    if (*field != v) {
        *field = v;
        out->chg |= SC_PROTO_CHG_ROOMS;
        out->rooms_changed |= 1 << a;
    }
}

static void _room_demand(sc_proto_t *p, int a, bool d, sc_proto_out_t *out)
{
    sc_proto_room_t *r = &p->room[a];
    if (!r->seen || r->demand != d) {
        r->seen = true;
        r->demand = d;
        out->chg |= SC_PROTO_CHG_ROOMS;
        out->rooms_changed |= 1 << a;
    }
}

/* Item in notify / forward numbering → room state */
static void _room_item(sc_proto_t *p, int a, uint8_t item, const uint8_t *v, int nval, sc_proto_out_t *out)
{
    sc_proto_room_t *r = &p->room[a];
    if (nval < 1) return;
    if (item == _ITEM_MODE || item == _ITEM_MODE_ALT) {
        uint8_t mode = v[0];
        if (item == _ITEM_MODE_ALT && !(mode & 0x20) && mode != SC_PROTO_MODE_TIMER) mode |= 0x20;
        _room_set(p, a, &r->mode, mode, out);
    } else if (item == _ITEM_SET) {
        _room_set(p, a, &r->set2, v[0], out);
    }
}

/* Cn answering a 94 read: read numbering */
static void _room_read_answer(sc_proto_t *p, int a, uint8_t item, const uint8_t *v, int nval, sc_proto_out_t *out)
{
    sc_proto_room_t *r = &p->room[a];
    if (item == 0x00 && nval >= 1) _room_set(p, a, &r->mode, v[0], out);
    else if (item == 0x02 && nval >= 2) { _room_set(p, a, &r->mode, v[0], out); _room_set(p, a, &r->set2, v[1], out); }
}

static void _boiler(sc_proto_t *p, uint8_t addr_byte, sc_proto_out_t *out)
{
    bool b = addr_byte & _ADDR_BOILER;
    if (b != p->boiler) {
        p->boiler = b;
        out->chg |= SC_PROTO_CHG_BOILER;
    }
}

void sc_proto_rx(sc_proto_t *p, const uint8_t *m, int len, int64_t now_ms, sc_proto_out_t *out)
{
    if (len < 3) return;
    uint8_t type = m[0] & 0xF0;
    int a = m[1] & _ADDR_MASK;
    bool ours = p->addr && a == p->addr;
    int nval = len - 4;                         /* after header, address, item */

    if (a > SC_PROTO_ADDR_MAX) return;

    switch (type) {
    /* --- MC200 → controllers: all carry the boiler bit --- */
    case 0x80:                                  /* 83 <addr> poll */
        _boiler(p, m[1], out);
        p->read_item[a] = _NO_READ;
        if (ours) {
            p->registered = m[1] & _ADDR_REGISTERED;
            _answer_poll(p, now_ms, out);
        }
        break;
    case 0x90:                                  /* 94 <addr> <item> read */
        _boiler(p, m[1], out);
        if (len < 4) break;
        p->read_item[a] = m[2];
        if (ours) _answer_read(p, m[2], now_ms, out);
        break;
    case 0xA0:                                  /* A5 forward, or A5~A7 answering a D4 */
        _boiler(p, m[1], out);
        if (len < 5) break;
        if (a == SC_PROTO_ADDR_LIVING && m[2] >= _MC200_ITEM_MIN) {
            if (m[2] == _MC200_HOT_WATER) p->hot_water2 = m[3];
            break;
        }
        if (ours) {
            _apply(p, m[2], m[3], out);
            _e4(p, now_ms, &out->tx);           /* A5 takes the poll's place */
        } else {
            _room_item(p, a, m[2], &m[3], nval, out);
        }
        break;
    case 0xB0:                                  /* B3 <addr> ack of our notify */
        _boiler(p, m[1], out);
        if (ours && p->n_notify) {
            memmove(&p->notify[0], &p->notify[1], sizeof(p->notify[0]) * (p->n_notify - 1));
            p->n_notify--;
        }
        break;

    /* --- controllers → MC200: all carry their demand bit --- */
    case 0xE0:                                  /* E4 <addr|kind> <value> */
        if (ours || len < 4) break;
        _room_demand(p, a, m[1] & _ADDR_DEMAND, out);
        if (m[1] & _ADDR_E4_TEMP) {
            _room_set(p, a, &p->room[a].temp2, m[2], out);
        } else if (!p->hour_heard && m[2] <= 0x1F) {
            p->hour = m[2];                     /* start where the bus is, then count alone */
            p->hour_ms = now_ms;
            p->hour_heard = true;
        }
        break;
    case 0xC0:                                  /* Cn notify, or answer to a 94 read */
        if (ours || a == 0 || len < 5) break;
        _room_demand(p, a, m[1] & _ADDR_DEMAND, out);
        if (m[2] == _ITEM_FULL && nval >= 3) {
            _room_set(p, a, &p->room[a].mode, m[3], out);
            _room_set(p, a, &p->room[a].set2, m[4], out);
            _room_set(p, a, &p->room[a].temp2, m[5], out);
        } else if (p->read_item[a] == m[2]) {
            _room_read_answer(p, a, m[2], &m[3], nval, out);
        } else {
            _room_item(p, a, m[2], &m[3], nval, out);
        }
        p->read_item[a] = _NO_READ;
        break;
    default:                                    /* D4 · F5 from the DT-200: the A5 that follows says it */
        break;
    }
}

/* ===== Local changes ===== */

void sc_proto_set_mode(sc_proto_t *p, uint8_t mode, sc_proto_out_t *out)
{
    if (p->mode == mode) return;
    p->mode = mode;
    _notify(p, _ITEM_MODE, mode);
    _update_demand(p, out);
}

void sc_proto_set_setpoint(sc_proto_t *p, uint8_t set2, sc_proto_out_t *out)
{
    if (p->set2 == set2) return;
    p->set2 = set2;
    _notify(p, _ITEM_SET, set2);
    _update_demand(p, out);
}

void sc_proto_set_temp(sc_proto_t *p, int16_t temp2, sc_proto_out_t *out)
{
    p->temp2 = temp2;
    _update_demand(p, out);
}

bool sc_proto_request_set(sc_proto_t *p, uint8_t addr, uint8_t item, uint8_t value)
{
    if (!_living(p) || addr < 2 || addr > SC_PROTO_ADDR_MAX) return false;
    /* A newer value for the same room and item replaces the waiting one */
    for (int i = 0; i < p->n_request; i++) {
        sc_proto_msg_t *r = &p->request[i];
        if ((r->b[0] & 0xF0) == 0xF0 && r->b[1] == addr && r->b[2] == item) {
            r->b[3] = value;
            sc_proto_seal(r);
            return true;
        }
    }
    if (p->n_request == SC_PROTO_REQUEST_MAX) return false;
    _msg(&p->request[p->n_request++], 5, 0xF0, addr, item, value, 0);
    return true;
}
