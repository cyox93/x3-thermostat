#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "argtable3/argtable3.h"
#include "driver/uart.h"

#include "eventloop.h"
#include "plc.h"
#include "plc_proto.h"
#include "board.h"

#include "main.h"

static const char* TAG = "plcloop";

/* Event ids */
#define PLCLOOP_RX_EVENT_ID         1   /* frame decoded (ISR) */
#define PLCLOOP_RX_ERROR_EVENT_ID   2   /* burst that was not a frame (ISR) */
#define PLCLOOP_TX_DONE_EVENT_ID    3   /* send finished (ISR) */
#define PLCLOOP_SEND_EVENT_ID       4   /* sc_plcloop_send */
#define PLCLOOP_ROLE_EVENT_ID       5   /* sc_plcloop_set_role / set_heat / set_temp / request_set */

/* Timer ids */
static const int PLCLOOP_GROUP_TIMER_ID = 1;
static const int PLCLOOP_ANSWER_TIMER_ID = 3;
#if CONFIG_SC_PLC_MC200_STANDIN
static const int PLCLOOP_MC200_TIMER_ID = 2;
#endif

/* Frames arrive a whole message at a time, when the message ends. An answer
 * then lands 60~95 ms after its poll, and the next poll ~127 ms or more after
 * that (docs/plc-protocol.md §2): 100 ms of silence closes a group. */
#define _GROUP_GAP_MS       100
#define _GROUP_MAX          48      /* a poll with a request and two answers is 17 */

/*
 * Message framing, inferred from the bus (docs/plc-protocol.md §5.2):
 *   header  high nibble = message type (bit 7 always set), low nibble =
 *           message length including header and checksum
 *   last    checksum = sum of the preceding bytes & 0x7F
 * Data bytes can have bit 7 set too, so only the length finds the next
 * header. A group is one MC200 poll with whatever answers follow it.
 */
#define _MSG_HEADER         0x80
#define _MSG_LEN_MASK       0x0F
#define _MSG_SUM_MASK       0x7F

/* A message closes ~5 ms after its last frame (RMT idle). Real controllers'
 * answers land 60~95 ms after the poll (§2); starting ours 20 ms after the
 * poll closes puts it at ~73 ms. */
#define _ANSWER_DELAY_MS    20

/* Temporary: every complete group as a hex line on UART0 (J3), so the bus
 * can be logged by another machine while decoding the codes. */
#define _DUMP_UART          UART_NUM_0
#define _DUMP_BAUD          115200

#if CONFIG_SC_PLC_MC200_STANDIN
/*
 * Bench only (CONFIG_SC_PLC_MC200_STANDIN, off in the real firmware): MC200
 * stand-in for a bus with one DT-200 or LT200SPD, to see whether a device
 * accepts our frames. Answers are replayed from the bus log
 * (docs/plc-protocol.md §5.6); nothing here is policy.
 */
#define _MC200_TICK_MS      10
#define _MC200_PERIOD_US    254000  /* poll to poll, §2 */
#define _MC200_QUIET_US     130000  /* after the last traffic, before the next poll */
#define _MC200_DROP_US      9000000 /* unanswered this long → deregistered, §5.6 */
#define _MC200_DT200        0x01    /* DT-200 address; a room is its number + 1 */
#define _MC200_REGISTERED   0x40
#define _MC200_HOT_WATER    0x78    /* item 3B value as logged; meaning inferred */
#endif

typedef struct _plc_send_data {
    uint8_t len;
    uint8_t data[SC_PLC_SEND_MAX];
} _plc_send_data_t;

typedef enum {
    _ROLE_OP_ROLE,
    _ROLE_OP_HEAT,
    _ROLE_OP_TEMP,
    _ROLE_OP_REQUEST,
} _role_op_t;

typedef struct _role_cmd {
    _role_op_t op;
    uint8_t a, b, c;
    int16_t v;
} _role_cmd_t;

typedef struct _plc_group {
    uint8_t data[_GROUP_MAX];
    int len;
} _plc_group_t;

typedef struct _plcloop {
    sc_eventloop_t loop;

    _plc_group_t cur;           /* group being received */
    _plc_group_t last;          /* last complete group */
    int64_t last_us;            /* when it completed */
    bool monitor;               /* log every frame */

    uint32_t frames;
    uint32_t groups;
    uint32_t group_changes;
    uint32_t overflows;         /* frames beyond _GROUP_MAX */
    uint32_t messages;
    uint32_t bad_messages;      /* wrong length, checksum or stray byte */
    uint32_t echoes;
    uint32_t err_no_start;
    uint32_t err_bits;
    uint32_t sent;
    _plc_send_data_t sending;   /* on the wire, joins the group when done */

    int64_t bus_us;             /* last traffic, ours or not */

    /* Controller role (components/plcproto) */
    sc_proto_t proto;
    uint8_t rx_msg[SC_PROTO_MSG_MAX];
    int rx_len;
    sc_proto_msg_t answer;      /* waiting for _ANSWER_DELAY_MS */
    uint32_t answered;
    sc_plcloop_callback_t cb;
    void *cb_arg;
#if CONFIG_SC_PLC_MC200_STANDIN
    /* Bench MC200 stand-in */
    bool mc200;
    bool mc200_registered;
    uint8_t mc200_addr;         /* the one device polled */
    uint8_t mc200_rx[16];       /* message being received */
    int mc200_rx_len;
    int64_t poll_us;            /* last poll */
    int64_t dev_us;             /* last message from the device */
    _plc_send_data_t mc200_pending;   /* plc_send waiting for the next poll slot */
    uint32_t mc200_polls;
    uint32_t mc200_answers;
#endif
} _plcloop_t;

static _plcloop_t *_plcloop;

/* Group as messages, e.g. "83 01 04 | E4 03 00 67". A message that fails
 * the framing rules is marked with '!'. Returns the number of bad messages. */
static int _group_str(const _plc_group_t *g, char *buf, size_t len, int *count)
{
    size_t n = 0;
    int bad = 0;
    int msgs = 0;
    int i = 0;

    buf[0] = '\0';
    while (i < g->len && n < len) {
        int mlen = 1;
        bool ok = false;

        if (g->data[i] & _MSG_HEADER) {
            mlen = g->data[i] & _MSG_LEN_MASK;
            if (mlen >= 2 && i + mlen <= g->len) {
                uint32_t sum = 0;
                for (int k = 0; k < mlen - 1; k++) sum += g->data[i + k];
                ok = (sum & _MSG_SUM_MASK) == g->data[i + mlen - 1];
            } else {
                mlen = g->len - i;
            }
        }

        n += snprintf(buf + n, len - n, "%s", msgs ? " | " : "");
        for (int k = 0; k < mlen && n < len; k++) {
            n += snprintf(buf + n, len - n, "%s%02X", k ? " " : "", g->data[i + k]);
        }
        if (!ok && n < len) {
            n += snprintf(buf + n, len - n, "!");
            bad++;
        }
        msgs++;
        i += mlen;
    }
    if (count) *count = msgs;
    return bad;
}

/* ========== Driver callback — hand off to the plcloop thread ========== */

/**
 * PLC callback - runs in ISR context
 */
static void _plc_driver_cb(sc_plc_event_t event, int value, void *arg)
{
    if (!_plcloop || !_plcloop->loop) return;

    int id;
    switch (event) {
    case SC_PLC_EVENT_RX:       id = PLCLOOP_RX_EVENT_ID; break;
    case SC_PLC_EVENT_RX_ERROR: id = PLCLOOP_RX_ERROR_EVENT_ID; break;
    case SC_PLC_EVENT_TX_DONE:  id = PLCLOOP_TX_DONE_EVENT_ID; break;
    default: return;
    }
    sc_eventloop_post_event_from_isr(_plcloop->loop, id, &value, sizeof(value));
}

/* ========== Event handlers — run in the plcloop thread ========== */

static void _group_timer_cb(int id, void *arg, void *data)
{
    _plc_group_t *cur = &_plcloop->cur;
    if (cur->len == 0) return;

    _plcloop->groups++;
    _plcloop->last_us = esp_timer_get_time();

    char str[_GROUP_MAX * 5];
    int msgs = 0;
    int bad = _group_str(cur, str, sizeof(str), &msgs);
    _plcloop->messages += msgs;
    _plcloop->bad_messages += bad;

    /* Every group, with the time in ms, for logging on another machine */
    char line[sizeof(str) + 16];
    int n = snprintf(line, sizeof(line), "%lld %s\r\n", _plcloop->last_us / 1000, str);
    uart_write_bytes(_DUMP_UART, line, n);

    /* The bus repeats the same group ~4 times a second: log changes only */
    if (cur->len != _plcloop->last.len || memcmp(cur->data, _plcloop->last.data, cur->len)) {
        ESP_LOGI(TAG, "group %s", str);
        _plcloop->group_changes++;
    }

    _plcloop->last = *cur;
    cur->len = 0;
    _plcloop->rx_len = 0;       /* a message never spans groups */
#if CONFIG_SC_PLC_MC200_STANDIN
    /* A message never spans groups */
    _plcloop->mc200_rx_len = 0;
#endif
}

static void _group_add(uint8_t byte)
{
    _plc_group_t *cur = &_plcloop->cur;
    if (cur->len < _GROUP_MAX) {
        cur->data[cur->len++] = byte;
    } else {
        _plcloop->overflows++;
    }
    sc_eventloop_add_timer(_plcloop->loop, PLCLOOP_GROUP_TIMER_ID, _group_timer_cb, NULL, _GROUP_GAP_MS, false);
}

/* Send from the loop thread. What we send joins the group once it is on
 * the bus, so the dump shows both sides in bus order; its echo is dropped. */
static esp_err_t _send(const uint8_t *data, size_t len)
{
    /* TODO: listen before talk — wait for a quiet bus between groups */
    esp_err_t ret = sc_plc_send(data, len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(ret));
        return ret;
    }
    _plcloop->sending.len = len;
    memcpy(_plcloop->sending.data, data, len);
    _plcloop->bus_us = esp_timer_get_time();
    return ESP_OK;
}

#if CONFIG_SC_PLC_MC200_STANDIN

/* ========== Bench MC200 stand-in ========== */

/* Fill in the header length and checksum, then send */
static esp_err_t _mc200_send(uint8_t *msg, int len)
{
    uint32_t sum = 0;
    msg[0] = (msg[0] & 0xF0) | len;
    for (int i = 0; i < len - 1; i++) sum += msg[i];
    msg[len - 1] = sum & _MSG_SUM_MASK;
    return _send(msg, len);
}

/* DT-200 reads an MC200 item: answer with what the MC200 answered on the
 * real bus (lt200spd log). Unknown items get no answer. */
static void _mc200_read(uint8_t item)
{
    uint8_t m[8] = { 0, _MC200_DT200, item };

    switch (item) {
    case 0x1E: { /* connect: ack, then read its full state as the MC200 did */
        uint8_t both[7] = { 0xB3, _MC200_REGISTERED | _MC200_DT200, 0, 0x94, _MC200_DT200, 0x1E, 0 };
        both[2] = (both[0] + both[1]) & _MSG_SUM_MASK;
        both[6] = (both[3] + both[4] + both[5]) & _MSG_SUM_MASK;
        _plcloop->mc200_registered = true;
        if (_send(both, sizeof(both)) == ESP_OK) _plcloop->mc200_answers++;
        return;
    }
    case 0x3B: m[0] = 0xA0; m[3] = _MC200_HOT_WATER; m[4] = 0x00; _mc200_send(m, 6); break;
    case 0x39: m[0] = 0xA0; m[3] = 0x00; _mc200_send(m, 5); break;
    case 0x36: m[0] = 0xA0; m[3] = 0x02; _mc200_send(m, 5); break;
    case 0x37: m[0] = 0xA0; m[3] = 0x04; _mc200_send(m, 5); break;
    case 0x3F: m[0] = 0xA0; m[3] = 0x22; _mc200_send(m, 5); break;
    case 0x3D: m[0] = 0xA0; m[3] = 0x12; m[4] = 0x78; m[5] = 0xA0; _mc200_send(m, 7); break;
    case 0x3E: m[0] = 0xA0; m[3] = 0x12; m[4] = 0x01; m[5] = 0x0C; _mc200_send(m, 7); break;
    default:
        ESP_LOGW(TAG, "mc200: no answer for item %02X", item);
        return;
    }
    _plcloop->mc200_answers++;
}

static void _mc200_message(const uint8_t *m, int len)
{
    _plcloop->dev_us = esp_timer_get_time();
    /* A room that still counts itself registered (e.g. after our reboot)
     * answers the scan poll with E4 instead of CA: any answer registers it */
    bool was_registered = _plcloop->mc200_registered;
    _plcloop->mc200_registered = true;

    switch (m[0] & 0xF0) {
    case 0xD0:  /* D4 <addr> <item> */
        _mc200_read(m[2]);
        break;
    case 0xC0: { /* C5 / C6 notify, or CA full state that registers a room: ack.
                  * The DT-200's CA answers our 94 read and gets none. */
        if ((m[0] & 0x0F) == 0x0A && was_registered) break;
        uint8_t ack[3] = { 0xB0, _MC200_REGISTERED | _plcloop->mc200_addr };
        if (_mc200_send(ack, 3) == ESP_OK) _plcloop->mc200_answers++;
        break;
    }
    default:    /* E4 answer, F5 for a room we don't have */
        break;
    }
}

/* Gather one received byte into a message; handle it when complete */
static void _mc200_rx(uint8_t byte)
{
    uint8_t *m = _plcloop->mc200_rx;
    int *n = &_plcloop->mc200_rx_len;

    if (*n == 0 && !(byte & _MSG_HEADER)) return;
    m[(*n)++] = byte;

    int len = m[0] & _MSG_LEN_MASK;
    if (len < 2 || len > (int)sizeof(_plcloop->mc200_rx)) {
        *n = 0;
        return;
    }
    if (*n < len) return;
    *n = 0;

    uint32_t sum = 0;
    for (int i = 0; i < len - 1; i++) sum += m[i];
    if ((sum & _MSG_SUM_MASK) != m[len - 1]) return;
    _mc200_message(m, len);
}

static void _mc200_timer_cb(int id, void *arg, void *data)
{
    int64_t now = esp_timer_get_time();

    if (_plcloop->mc200_registered && now - _plcloop->dev_us > _MC200_DROP_US) {
        ESP_LOGW(TAG, "mc200: DT-200 silent, deregistered");
        _plcloop->mc200_registered = false;
    }
    if (sc_plc_tx_busy()) return;
    if (now - _plcloop->poll_us < _MC200_PERIOD_US) return;
    if (now - _plcloop->bus_us < _MC200_QUIET_US) return;

    _plcloop->poll_us = now;

    /* A setting goes out in place of the poll, as the MC200 sends A5 */
    _plc_send_data_t *pending = &_plcloop->mc200_pending;
    if (pending->len) {
        _send(pending->data, pending->len);
        pending->len = 0;
        return;
    }

    uint8_t poll[3] = { 0x80, (_plcloop->mc200_registered ? _MC200_REGISTERED : 0) | _plcloop->mc200_addr };
    _plcloop->mc200_polls++;
    _mc200_send(poll, 3);
}

#endif

/* ========== Controller role ========== */

static void _note(sc_plc_note_kind_t kind, uint8_t addr)
{
    if (!_plcloop->cb) return;
    const sc_proto_t *p = &_plcloop->proto;
    sc_plc_note_t n = { .kind = kind, .addr = addr, .boiler = p->boiler };
    if (kind == SC_PLC_NOTE_ROOM) {
        const sc_proto_room_t *r = &p->room[addr];
        n.demand = r->demand;
        n.temp2 = r->temp2;
        n.set2 = r->set2;
        n.mode = r->mode;
    } else {
        n.addr = p->addr;
        n.demand = p->demand;
        n.temp2 = p->temp2;
        n.set2 = p->set2;
        n.mode = p->mode;
    }
    _plcloop->cb(&n, _plcloop->cb_arg);
}

static void _notes(const sc_proto_out_t *out)
{
    if (out->chg & SC_PROTO_CHG_OWN) _note(SC_PLC_NOTE_OWN, 0);
    if (out->chg & SC_PROTO_CHG_ROOMS) {
        for (int a = 0; a <= SC_PROTO_ADDR_MAX; a++) {
            if (out->rooms_changed & (1 << a)) _note(SC_PLC_NOTE_ROOM, a);
        }
    }
    if (out->chg & (SC_PROTO_CHG_BOILER | SC_PROTO_CHG_DEMAND)) _note(SC_PLC_NOTE_STATE, 0);
}

static void _msg_str(const sc_proto_msg_t *m, char *buf, size_t len)
{
    size_t n = 0;
    buf[0] = '\0';
    for (int i = 0; i < m->len && n < len; i++) n += snprintf(buf + n, len - n, "%s%02X", i ? " " : "", m->b[i]);
}

static void _answer_timer_cb(int id, void *arg, void *data)
{
    sc_proto_msg_t *m = &_plcloop->answer;
    if (!m->len) return;

    if (_send(m->b, m->len) == ESP_OK) _plcloop->answered++;
    /* The plain poll answer every 254 ms would flood the log */
    if ((m->b[0] & 0xF0) != 0xE0) {
        char str[SC_PROTO_MSG_MAX * 3 + 1];
        _msg_str(m, str, sizeof(str));
        ESP_LOGI(TAG, "sent %s", str);
    }
    m->len = 0;
}

static void _proto_message(const uint8_t *m, int len)
{
    sc_proto_out_t out = { 0 };
    sc_proto_rx(&_plcloop->proto, m, len, esp_timer_get_time() / 1000, &out);
    if (out.tx.len) {
        _plcloop->answer = out.tx;
        sc_eventloop_add_timer(_plcloop->loop, PLCLOOP_ANSWER_TIMER_ID, _answer_timer_cb, NULL, _ANSWER_DELAY_MS, false);
    }
    _notes(&out);
}

/* Gather received bytes into messages by the header's length */
static void _proto_rx(uint8_t byte)
{
    uint8_t *m = _plcloop->rx_msg;
    int *n = &_plcloop->rx_len;

    if (*n == 0 && !(byte & _MSG_HEADER)) return;
    m[(*n)++] = byte;

    int len = m[0] & _MSG_LEN_MASK;
    if (len < 2) {
        *n = 0;
        return;
    }
    if (*n < len) return;
    *n = 0;

    uint32_t sum = 0;
    for (int i = 0; i < len - 1; i++) sum += m[i];
    if ((sum & _MSG_SUM_MASK) != m[len - 1]) return;
    _proto_message(m, len);
}

static void _role_event_handler(int id, void *arg, void *data)
{
    _role_cmd_t *c = (_role_cmd_t *)data;
    sc_proto_t *p = &_plcloop->proto;
    sc_proto_out_t out = { 0 };

    switch (c->op) {
    case _ROLE_OP_ROLE: {
        int16_t temp2 = p->temp2;
        sc_proto_init(p, c->a, c->b, c->c);
        sc_proto_set_temp(p, temp2, NULL);
        _plcloop->answer.len = 0;
        ESP_LOGI(TAG, "role: %s, address %d", c->a == 0 ? "listen only" :
                 c->a == SC_PROTO_ADDR_LIVING ? "living room" : "room", c->a);
        out.chg |= SC_PROTO_CHG_DEMAND;     /* report our state afresh */
        break;
    }
    case _ROLE_OP_HEAT:
        sc_proto_set_mode(p, c->a, &out);
        sc_proto_set_setpoint(p, c->b, &out);
        break;
    case _ROLE_OP_TEMP:
        sc_proto_set_temp(p, c->v, &out);
        break;
    case _ROLE_OP_REQUEST:
        if (!sc_proto_request_set(p, c->a, c->b, c->c)) {
            ESP_LOGW(TAG, "request to %d not queued (not the living room, or full)", c->a);
        }
        break;
    }
    _notes(&out);
}

static void _rx_event_handler(int id, void *arg, void *data)
{
    int value = *(int *)data;
    uint8_t byte = value & 0xff;

    _plcloop->bus_us = esp_timer_get_time();
    if (value & SC_PLC_RX_ECHO) {
        _plcloop->echoes++;
        if (_plcloop->monitor) printf("plc: echo %02X\n", byte);
        return;
    }

    _plcloop->frames++;
    if (_plcloop->monitor) printf("plc: rx %02X\n", byte);

    _group_add(byte);
    _proto_rx(byte);
#if CONFIG_SC_PLC_MC200_STANDIN
    if (_plcloop->mc200) _mc200_rx(byte);
#endif
}

/* Raw pulses of a burst that did not decode, as one UART0 line:
 * "<ms> # no-start H901 L118 H71 ..." (H = carrier on, L = off, µs) */
static void _dump_error_burst(const char *what)
{
    /* A few frames' worth is enough to see what went wrong */
    static uint16_t burst[400];
    static char line[sizeof(burst) / sizeof(burst[0]) * 7 + 48];

    size_t n = sc_plc_get_error_burst(burst, sizeof(burst) / sizeof(burst[0]));
    int len = snprintf(line, sizeof(line), "%lld # %s", esp_timer_get_time() / 1000, what);
    for (size_t i = 0; i < n && len < (int)sizeof(line) - 12; i++) {
        len += snprintf(line + len, sizeof(line) - len, " %c%u",
                        burst[i] & SC_PLC_BURST_LEVEL ? 'H' : 'L', burst[i] & 0x7fff);
    }
    len += snprintf(line + len, sizeof(line) - len, "\r\n");
    uart_write_bytes(_DUMP_UART, line, len);
}

static void _rx_error_event_handler(int id, void *arg, void *data)
{
    int err = *(int *)data;

    _dump_error_burst(err == SC_PLC_RX_ERR_NO_START ? "no-start" : "bits");

    if (err == SC_PLC_RX_ERR_NO_START) {
        _plcloop->err_no_start++;
    } else {
        _plcloop->err_bits++;
    }
    if (_plcloop->monitor) printf("plc: rx error %s\n", err == SC_PLC_RX_ERR_NO_START ? "no-start" : "bits");
}

static void _tx_done_event_handler(int id, void *arg, void *data)
{
    _plcloop->sent++;
    _plcloop->bus_us = esp_timer_get_time();

    /* Added at the end, not the start: a send takes 12 ms a byte and the
     * answer comes 60~95 ms after it, so the group gap must count from here */
    for (int i = 0; i < _plcloop->sending.len; i++) _group_add(_plcloop->sending.data[i]);
    _plcloop->sending.len = 0;
    if (_plcloop->monitor) printf("plc: tx done\n");
}

static void _send_event_handler(int id, void *arg, void *data)
{
    _plc_send_data_t *send = (_plc_send_data_t *)data;

#if CONFIG_SC_PLC_MC200_STANDIN
    /* While standing in for the MC200 the bus is ours: wait for a poll slot */
    if (_plcloop->mc200) {
        _plcloop->mc200_pending = *send;
        return;
    }
#endif
    _send(send->data, send->len);
}

/* ========== API ========== */

esp_err_t sc_plcloop_send(const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(_plcloop, ESP_ERR_INVALID_STATE, TAG, "plcloop not initialized");
    ESP_RETURN_ON_FALSE(data && len > 0 && len <= SC_PLC_SEND_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid length");

    _plc_send_data_t send = { .len = len };
    memcpy(send.data, data, len);
    return sc_eventloop_post_event(_plcloop->loop, PLCLOOP_SEND_EVENT_ID, &send, sizeof(send));
}

static esp_err_t _role_post(const _role_cmd_t *c)
{
    ESP_RETURN_ON_FALSE(_plcloop, ESP_ERR_INVALID_STATE, TAG, "plcloop not initialized");
    return sc_eventloop_post_event(_plcloop->loop, PLCLOOP_ROLE_EVENT_ID, (void *)c, sizeof(*c));
}

esp_err_t sc_plcloop_set_role(uint8_t addr, uint8_t mode, uint8_t set2)
{
    ESP_RETURN_ON_FALSE(addr <= SC_PROTO_ADDR_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid address %d", addr);
    return _role_post(&(_role_cmd_t){ .op = _ROLE_OP_ROLE, .a = addr, .b = mode, .c = set2 });
}

esp_err_t sc_plcloop_set_heat(uint8_t mode, uint8_t set2)
{
    return _role_post(&(_role_cmd_t){ .op = _ROLE_OP_HEAT, .a = mode, .b = set2 });
}

esp_err_t sc_plcloop_set_temp(int16_t temp2)
{
    return _role_post(&(_role_cmd_t){ .op = _ROLE_OP_TEMP, .v = temp2 });
}

esp_err_t sc_plcloop_request_set(uint8_t addr, uint8_t item, uint8_t value)
{
    return _role_post(&(_role_cmd_t){ .op = _ROLE_OP_REQUEST, .a = addr, .b = item, .c = value });
}

esp_err_t sc_plcloop_register_callback(sc_plcloop_callback_t callback, void *arg)
{
    ESP_RETURN_ON_FALSE(_plcloop, ESP_ERR_INVALID_STATE, TAG, "plcloop not initialized");
    _plcloop->cb_arg = arg;
    _plcloop->cb = callback;
    return ESP_OK;
}

/* ========== Console ========== */

static int _plc_cmd(int argc, char **argv)
{
    char str[_GROUP_MAX * 5];
    _group_str(&_plcloop->last, str, sizeof(str), NULL);

    printf("last group : %s", _plcloop->last.len ? str : "-");
    if (_plcloop->last.len) {
        printf(" (%lld ms ago)", (esp_timer_get_time() - _plcloop->last_us) / 1000);
    }
    printf("\n");
    printf("frames     : %lu in %lu groups, %lu changes, %lu overflow\n",
           _plcloop->frames, _plcloop->groups, _plcloop->group_changes, _plcloop->overflows);
    printf("messages   : %lu, %lu bad (length / checksum)\n", _plcloop->messages, _plcloop->bad_messages);
    printf("errors     : %lu no-start, %lu bad bits\n", _plcloop->err_no_start, _plcloop->err_bits);
    printf("sent       : %lu sends, %lu echo frames\n", _plcloop->sent, _plcloop->echoes);
    printf("carrier    : %s\n", sc_plc_carrier() ? "on" : "off");

    const sc_proto_t *p = &_plcloop->proto;
    printf("role       : %s, address %d, %s, %lu answers sent\n",
           p->addr == 0 ? "listen only" : p->addr == SC_PROTO_ADDR_LIVING ? "living room" : "room",
           p->addr, p->registered ? "registered" : "not registered", _plcloop->answered);
    char room[8] = "-", hot[8] = "-";
    if (p->temp2 != SC_PROTO_TEMP_NONE) snprintf(room, sizeof(room), "%.1f", p->temp2 / 2.0);
    if (p->hot_water2 != SC_PROTO_TEMP_NONE) snprintf(hot, sizeof(hot), "%.1f", p->hot_water2 / 2.0);
    printf("ours       : mode %02X, set %.1f, room %s, demand %s, notify %d, hour %d\n",
           p->mode, p->set2 / 2.0, room, p->demand ? "on" : "off", p->n_notify, p->hour);
    printf("boiler     : %s, hot water %s\n", p->boiler ? "on" : "off", hot);
    for (int a = 1; a <= SC_PROTO_ADDR_MAX; a++) {
        const sc_proto_room_t *r = &p->room[a];
        if (!r->seen) continue;
        printf("addr %d     : room %5.1f, set %5.1f, mode %02X, demand %s\n", a,
               r->temp2 == SC_PROTO_TEMP_NONE ? 0 : r->temp2 / 2.0, r->set2 == SC_PROTO_TEMP_NONE ? 0 : r->set2 / 2.0,
               r->mode < 0 ? 0 : r->mode, r->demand ? "on" : "off");
    }
    return 0;
}

static struct { struct arg_str *state; struct arg_end *end; } _plc_mon_args;

static int _plc_mon_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_plc_mon_args) != 0) {
        arg_print_errors(stderr, _plc_mon_args.end, argv[0]);
        return 1;
    }
    _plcloop->monitor = strcmp(_plc_mon_args.state->sval[0], "on") == 0;
    printf("plc monitor %s\n", _plcloop->monitor ? "on" : "off");
    return 0;
}

static struct { struct arg_str *bytes; struct arg_end *end; } _plc_send_args;

static int _plc_send_cmd(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&_plc_send_args) != 0) {
        arg_print_errors(stderr, _plc_send_args.end, argv[0]);
        return 1;
    }

    uint8_t data[SC_PLC_SEND_MAX];
    int n = _plc_send_args.bytes->count;
    for (int i = 0; i < n; i++) {
        char *end;
        unsigned long v = strtoul(_plc_send_args.bytes->sval[i], &end, 16);
        if (*end || v > 0xff) {
            printf("invalid byte '%s' (hex 00~FF)\n", _plc_send_args.bytes->sval[i]);
            return 1;
        }
        data[i] = v;
    }
    return sc_plcloop_send(data, n) == ESP_OK ? 0 : 1;
}

#if CONFIG_SC_PLC_MC200_STANDIN
static struct { struct arg_str *state; struct arg_int *addr; struct arg_end *end; } _plc_mc200_args;

static int _plc_mc200_cmd(int argc, char **argv)
{
    if (argc == 1) {
        printf("mc200      : %s, address %d %s\n", _plcloop->mc200 ? "on" : "off", _plcloop->mc200_addr,
               _plcloop->mc200_registered ? "registered" : "not registered");
        printf("             %lu polls, %lu answers\n", _plcloop->mc200_polls, _plcloop->mc200_answers);
        return 0;
    }
    if (arg_parse(argc, argv, (void **)&_plc_mc200_args) != 0) {
        arg_print_errors(stderr, _plc_mc200_args.end, argv[0]);
        return 1;
    }
    bool on = strcmp(_plc_mc200_args.state->sval[0], "on") == 0;
    int addr = _plc_mc200_args.addr->count ? _plc_mc200_args.addr->ival[0] : _MC200_DT200;
    if (addr < 1 || addr > 6) {
        printf("address 1 (DT-200) or 2~6 (room + 1)\n");
        return 1;
    }
    if (on && (!_plcloop->mc200 || addr != _plcloop->mc200_addr)) {
        _plcloop->mc200_addr = addr;
        _plcloop->mc200_registered = false;
        _plcloop->mc200_rx_len = 0;
        sc_eventloop_add_timer(_plcloop->loop, PLCLOOP_MC200_TIMER_ID, _mc200_timer_cb, NULL, _MC200_TICK_MS, true);
    } else if (!on && _plcloop->mc200) {
        sc_eventloop_del_timer(_plcloop->loop, PLCLOOP_MC200_TIMER_ID);
    }
    _plcloop->mc200 = on;
    printf("mc200 %s\n", on ? "on" : "off");
    return 0;
}
#endif

static void _plcloop_cmd_init(void)
{
#if CONFIG_SC_PLC_MC200_STANDIN
    _plc_mc200_args.state = arg_str1(NULL, NULL, "<on|off>", "act as the MC200 (bench bus only)");
    _plc_mc200_args.addr = arg_int0(NULL, NULL, "<addr>", "device address, default 1 (DT-200)");
    _plc_mc200_args.end = arg_end(2);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "plc_mc200", .help = "Bench only: poll and answer one device as the MC200 does",
        .func = &_plc_mc200_cmd, .argtable = &_plc_mc200_args,
    }));
#endif

    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "plc", .help = "PLC bus status and controller role",
        .func = &_plc_cmd,
    }));

    _plc_mon_args.state = arg_str1(NULL, NULL, "<on|off>", "log every frame");
    _plc_mon_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "plc_mon", .help = "Log every PLC frame",
        .func = &_plc_mon_cmd, .argtable = &_plc_mon_args,
    }));

    _plc_send_args.bytes = arg_strn(NULL, NULL, "<hex>", 1, SC_PLC_SEND_MAX, "bytes, one frame each");
    _plc_send_args.end = arg_end(SC_PLC_SEND_MAX);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "plc_send", .help = "Send PLC frames, e.g. plc_send 83 01 04",
        .func = &_plc_send_cmd, .argtable = &_plc_send_args,
    }));
}

/* ========== Init ========== */

static esp_err_t _dump_uart_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = _DUMP_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    /* TX buffer so a write never waits on the wire — a raw burst line can
     * run to a couple of kB */
    ESP_RETURN_ON_ERROR(uart_driver_install(_DUMP_UART, 256, 4096, 0, NULL, 0), TAG, "uart install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(_DUMP_UART, &cfg), TAG, "uart config failed");
    return uart_set_pin(_DUMP_UART, BOARD_UART0_TX_PIN, BOARD_UART0_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void _plcloop_destroy(void)
{
    if (_plcloop) {
        if (_plcloop->loop) sc_eventloop_destroy(_plcloop->loop);
        free(_plcloop);
        _plcloop = NULL;
    }
}

esp_err_t sc_plcloop_init(void)
{
    esp_err_t ret = ESP_OK;

    if (_plcloop) {
        ESP_LOGW(TAG, "plcloop already initialized");
        return ESP_OK;
    }

    _plcloop = (_plcloop_t *)calloc(1, sizeof(_plcloop_t));
    ESP_RETURN_ON_FALSE(_plcloop, ESP_ERR_NO_MEM, TAG, "failed to allocate memory for plcloop");

    /* One receive can hold several frames, all posted from one ISR */
    _plcloop->loop = sc_eventloop_create("plcloop", 32, 4096);
    ESP_GOTO_ON_FALSE(_plcloop->loop, ESP_FAIL, _err, TAG, "failed to create plcloop");

    /* Register handlers before the driver callback can post */
    sc_eventloop_add_event(_plcloop->loop, PLCLOOP_RX_EVENT_ID, _rx_event_handler, NULL);
    sc_eventloop_add_event(_plcloop->loop, PLCLOOP_RX_ERROR_EVENT_ID, _rx_error_event_handler, NULL);
    sc_eventloop_add_event(_plcloop->loop, PLCLOOP_TX_DONE_EVENT_ID, _tx_done_event_handler, NULL);
    sc_eventloop_add_event(_plcloop->loop, PLCLOOP_SEND_EVENT_ID, _send_event_handler, NULL);
    sc_eventloop_add_event(_plcloop->loop, PLCLOOP_ROLE_EVENT_ID, _role_event_handler, NULL);

    /* Listen only until sysloop sets the role from the installed room */
    sc_proto_init(&_plcloop->proto, 0, SC_PROTO_MODE_HEAT, 44);

    ESP_GOTO_ON_ERROR(_dump_uart_init(), _err, TAG, "failed to init dump uart");

    sc_plc_register_callback(_plc_driver_cb, NULL);

    _plcloop_cmd_init();

    return ESP_OK;

_err:
    _plcloop_destroy();

    return ret;
}

void sc_plcloop_end(void)
{
    sc_plc_register_callback(NULL, NULL);
    _plcloop_destroy();
}
