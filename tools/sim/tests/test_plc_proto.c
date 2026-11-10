/* Controller roles against message sequences from the bus logs
   (docs/plc-protocol.md §5.6 · §5.8, §6.2 bench). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "plc_proto.h"

static sc_proto_t P;
static sc_proto_out_t O;

/* Feed one message given as bytes without the checksum; returns the answer */
static const sc_proto_msg_t *rx(int64_t ms, int n, ...)
{
    sc_proto_msg_t m = { .len = n + 1 };
    __builtin_va_list ap;
    __builtin_va_start(ap, n);
    for (int i = 0; i < n; i++) m.b[i] = (uint8_t)__builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    sc_proto_seal(&m);
    memset(&O, 0, sizeof(O));
    sc_proto_rx(&P, m.b, m.len, ms, &O);
    return &O.tx;
}

/* Answer equals these bytes (checksum checked separately by sealing a copy) */
static void is(const sc_proto_msg_t *m, int n, ...)
{
    sc_proto_msg_t want = { .len = n + 1 };
    __builtin_va_list ap;
    __builtin_va_start(ap, n);
    for (int i = 0; i < n; i++) want.b[i] = (uint8_t)__builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    sc_proto_seal(&want);
    if (m->len != want.len || memcmp(m->b, want.b, want.len)) {
        printf("got  :"); for (int i = 0; i < m->len; i++) printf(" %02X", m->b[i]); printf("\n");
        printf("want :"); for (int i = 0; i < want.len; i++) printf(" %02X", want.b[i]); printf("\n");
        assert(0);
    }
}

static void test_seal(void)
{
    /* 83 01 04 and E4 13 37 2E from the bus */
    sc_proto_msg_t m = { .len = 3, .b = { 0x80, 0x01 } };
    sc_proto_seal(&m);
    assert(m.b[0] == 0x83 && m.b[2] == 0x04);
    m = (sc_proto_msg_t){ .len = 4, .b = { 0xE0, 0x13, 0x37 } };
    sc_proto_seal(&m);
    assert(m.b[0] == 0xE4 && m.b[3] == 0x2E);
}

static void test_room_registers_and_answers(void)
{
    sc_proto_init(&P, 3, SC_PROTO_MODE_AWAY, 0x1C);        /* 방 2: 외출, 14.0 */
    sc_proto_set_temp(&P, 0x37, NULL);                       /* 27.5 */

    /* scan poll → full state, as LT200SPD: CA 03 1E 2B 1C 37 05 14 01 */
    is(rx(0, 2, 0x83, 0x03), 9, 0xCA, 0x03, 0x1E, 0x2B, 0x1C, 0x37, 0x05, 0x14, 0x01);
    assert(!P.registered);
    rx(10, 2, 0xB3, 0x43);                                   /* ack */
    /* registered polls: E4 alternates hour counter and temperature */
    is(rx(254, 2, 0x83, 0x43), 3, 0xE4, 0x03, 0x00);
    assert(P.registered);
    is(rx(508, 2, 0x83, 0x43), 3, 0xE4, 0x13, 0x37);
    /* other polls are not ours */
    assert(rx(600, 2, 0x83, 0x44)->len == 0);
}

static void test_hour_counter(void)
{
    sc_proto_init(&P, 3, SC_PROTO_MODE_HEAT, 44);
    P.registered = true;
    rx(0, 3, 0xE4, 0x01, 0x07);                              /* DT-200 says hour 7 */
    is(rx(10, 2, 0x83, 0x43), 3, 0xE4, 0x03, 0x07);          /* no temperature yet: counter only */
    /* an hour later on our own count */
    is(rx(60LL * 60 * 1000 + 20, 2, 0x83, 0x43), 3, 0xE4, 0x03, 0x08);
    /* later values from others do not move ours: devices drift apart (§5.8) */
    rx(60LL * 60 * 1000 + 30, 3, 0xE4, 0x02, 0x05);
    is(rx(60LL * 60 * 1000 + 40, 2, 0x83, 0x43), 3, 0xE4, 0x03, 0x08);
    /* 31 wraps to 0 */
    sc_proto_init(&P, 3, SC_PROTO_MODE_HEAT, 44);
    P.registered = true;
    rx(0, 3, 0xE4, 0x02, 0x1F);
    is(rx(60LL * 60 * 1000, 2, 0x83, 0x43), 3, 0xE4, 0x03, 0x00);
}

static void test_demand(void)
{
    sc_proto_init(&P, 3, SC_PROTO_MODE_HEAT, 44);           /* 22.0 */
    memset(&O, 0, sizeof(O));
    sc_proto_set_temp(&P, 44, &O);                           /* 22.0: room ≥ set → off */
    assert(!P.demand && !(O.chg & SC_PROTO_CHG_DEMAND));
    sc_proto_set_temp(&P, 43, &O);                           /* 21.5 < 22.0 → on */
    assert(P.demand && (O.chg & SC_PROTO_CHG_DEMAND));
    sc_proto_set_mode(&P, SC_PROTO_MODE_AWAY, &O);           /* away 10 °C */
    assert(!P.demand);
    sc_proto_set_temp(&P, 19, &O);                           /* 9.5 < 10 → on in away */
    assert(P.demand);
    sc_proto_set_mode(&P, SC_PROTO_MODE_OFF, &O);
    assert(!P.demand);
    sc_proto_set_temp(&P, 10, &O);                           /* 5 °C: frost protection */
    assert(P.demand);
    sc_proto_set_temp(&P, SC_PROTO_TEMP_NONE, &O);           /* no sensor: no demand */
    assert(!P.demand);
}

static void test_notify_until_ack(void)
{
    sc_proto_init(&P, 3, SC_PROTO_MODE_HEAT, 60);           /* 30.0 */
    P.registered = true;
    sc_proto_set_temp(&P, 55, NULL);                         /* 27.5: demand on */
    memset(&O, 0, sizeof(O));
    sc_proto_set_setpoint(&P, 54, &O);                       /* 27.0: demand off */
    assert(!P.demand);
    /* C5 03 01 36 repeats until B3, then plain E4 */
    is(rx(0, 2, 0x83, 0x43), 4, 0xC5, 0x03, 0x01, 0x36);
    is(rx(254, 2, 0x83, 0x43), 4, 0xC5, 0x03, 0x01, 0x36);
    rx(260, 2, 0xB3, 0x43);
    assert(P.n_notify == 0);
    assert(rx(508, 2, 0x83, 0x43)->b[0] == 0xE4);
    /* demand bit rides on the notify: C5 23 … while demanding */
    sc_proto_set_setpoint(&P, 60, NULL);
    is(rx(762, 2, 0x83, 0x43), 4, 0xC5, 0x23, 0x01, 0x3C);
}

static void test_reads(void)
{
    sc_proto_init(&P, 3, SC_PROTO_MODE_AWAY, 0x1C);
    P.registered = true;
    is(rx(0, 3, 0x94, 0x43, 0x00), 4, 0xC5, 0x03, 0x00, 0x2B);
    is(rx(0, 3, 0x94, 0x43, 0x02), 5, 0xC6, 0x03, 0x02, 0x2B, 0x1C);
    is(rx(0, 3, 0x94, 0x43, 0x06), 4, 0xC5, 0x03, 0x06, 0x14);
    assert(rx(0, 3, 0x94, 0x43, 0x1E)->b[0] == 0xCA);
}

static void test_remote_set(void)
{
    /* bench: A5 03 01 3C while away → stored; A5 03 08 00 → heat 30 */
    sc_proto_init(&P, 3, SC_PROTO_MODE_AWAY, 0x1C);
    P.registered = true;
    sc_proto_set_temp(&P, 55, NULL);
    const sc_proto_msg_t *m = rx(0, 4, 0xA5, 0x03, 0x01, 0x3C);
    assert(m->b[0] == 0xE4 && (O.chg & SC_PROTO_CHG_OWN) && P.set2 == 0x3C && !P.demand);
    rx(254, 4, 0xA5, 0x03, 0x08, 0x00);                      /* all-off release: 00 → 20 */
    assert(P.mode == SC_PROTO_MODE_HEAT && P.demand && (O.chg & SC_PROTO_CHG_DEMAND));
    assert(P.n_notify == 0);                                  /* A5 is not echoed with C5 */
    rx(508, 4, 0xA5, 0x13, 0x08, 0x2A);                      /* OFF, boiler bit set */
    assert(P.mode == SC_PROTO_MODE_OFF && !P.demand && P.boiler);
}

static void test_living_room(void)
{
    sc_proto_init(&P, 1, SC_PROTO_MODE_HEAT, 44);
    sc_proto_set_temp(&P, 50, NULL);
    is(rx(0, 2, 0x83, 0x01), 9, 0xCA, 0x01, 0x1E, 0x20, 44, 50, 0x05, 0x14, 0x01);
    rx(5, 2, 0xB3, 0x41);
    /* first registered slot: D4 01 1E asks the MC200 to collect the rooms */
    is(rx(254, 2, 0x83, 0x41), 3, 0xD4, 0x01, 0x1E);
    /* then hot water reads, at most every 5 s */
    is(rx(508, 2, 0x83, 0x41), 3, 0xD4, 0x01, 0x3B);
    assert(rx(762, 2, 0x83, 0x41)->b[0] == 0xE4);
    rx(800, 5, 0xA6, 0x01, 0x3B, 0x90, 0x00);                /* hot water 72 °C */
    assert(P.hot_water2 == 0x90);

    /* setting another room: F5 in our slot, newest value wins */
    assert(sc_proto_request_set(&P, 3, 0x01, 0x3C));
    assert(sc_proto_request_set(&P, 3, 0x01, 0x3A));
    is(rx(1016, 2, 0x83, 0x41), 4, 0xF5, 0x03, 0x01, 0x3A);
    assert(!sc_proto_request_set(&P, 1, 0x01, 0x3A));       /* not to ourselves */

    /* overheard rooms */
    rx(1100, 3, 0xE4, 0x23, 0x30);                           /* room 2 demanding, hour */
    assert(P.room[3].demand && (O.rooms_changed & (1 << 3)));
    rx(1200, 3, 0xE4, 0x33, 0x31);                           /* 24.5 °C */
    assert(P.room[3].temp2 == 0x31);
    rx(1300, 4, 0xC5, 0x03, 0x01, 0x34);                     /* set 26.0 by the room */
    assert(P.room[3].set2 == 0x34);
    rx(1400, 3, 0x94, 0x44, 0x02);
    rx(1410, 5, 0xC6, 0x04, 0x02, 0x2B, 0x22);               /* room 3 read: away, 17.0 */
    assert(P.room[4].mode == SC_PROTO_MODE_AWAY && P.room[4].set2 == 0x22);
    rx(1500, 4, 0xA5, 0x14, 0x08, 0x00);                     /* MC200 forwards heat to room 3 */
    assert(P.room[4].mode == SC_PROTO_MODE_HEAT && P.boiler && (O.chg & SC_PROTO_CHG_BOILER));

    /* wallpad / app setting for the living room: A5 01 01 …, not a D4 answer */
    const sc_proto_msg_t *m = rx(1600, 4, 0xA5, 0x11, 0x01, 0x36);
    assert(P.set2 == 0x36 && (O.chg & SC_PROTO_CHG_OWN) && m->b[0] == 0xE4);
}

static void test_listen_only(void)
{
    sc_proto_init(&P, 0, SC_PROTO_MODE_HEAT, 44);
    assert(rx(0, 2, 0x83, 0x01)->len == 0);
    assert(rx(0, 2, 0x83, 0x40)->len == 0);                 /* address 0 is "all" */
    rx(10, 3, 0xE4, 0x11, 0x34);
    assert(P.room[1].temp2 == 0x34);
}

int main(void)
{
    test_seal();
    test_room_registers_and_answers();
    test_hour_counter();
    test_demand();
    test_notify_until_ack();
    test_reads();
    test_remote_set();
    test_living_room();
    test_listen_only();
    printf("plc_proto: all passed\n");
    return 0;
}
