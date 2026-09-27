#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "x3switch.h"

static const char *led_name(sc_x3switch_led_t m)
{
    switch (m) {
    case SC_X3SWITCH_LED_WHITE:  return "white";
    case SC_X3SWITCH_LED_ORANGE: return "orange";
    case SC_X3SWITCH_LED_BLINK:  return "blink";
    default:                     return "?";
    }
}

static int report(esp_err_t err)
{
    if (err != ESP_OK) {
        printf("x3switch: %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

static int print_status(bool relays, bool leds)
{
    sc_x3switch_status_t st;
    if (report(sc_x3switch_status(&st))) {
        return 1;
    }
    if (relays) {
        printf("relay: L1=%d L2=%d L3=%d L4=%d\n", st.relay[0], st.relay[1], st.relay[2], st.relay[3]);
    }
    if (leds) {
        for (int i = 0; i < SC_X3SWITCH_CH_NUM; i++) {
            printf("btn_led K%d: %-6s %u ms\n", i + 1, led_name(st.led[i]), st.led_interval_ms[i]);
        }
        printf("btn_led: %s, brightness %u %%\n", st.led_enable ? "on" : "off", st.led_brightness);
    }
    if (relays && leds) {
        printf("keys: K1=%d K2=%d K3=%d K4=%d\n", st.key[0], st.key[1], st.key[2], st.key[3]);
    }
    return 0;
}

/* x3switch [reset] */

static struct { struct arg_str *action; struct arg_end *end; } x3switch_args;

static int cmd_x3switch(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&x3switch_args) != 0) {
        arg_print_errors(stderr, x3switch_args.end, argv[0]);
        return 1;
    }
    if (x3switch_args.action->count > 0) {
        if (strcmp(x3switch_args.action->sval[0], "reset") == 0) {
            return report(sc_x3switch_reset());
        }
        printf("Invalid: '%s' (use reset)\n", x3switch_args.action->sval[0]);
        return 1;
    }

    char ver[32];
    if (report(sc_x3switch_version(ver, sizeof(ver)))) {
        return 1;
    }
    printf("version: %s\n", ver);
    return print_status(true, true);
}

void sc_x3switch_register_commands(void)
{
    x3switch_args.action = arg_str0(NULL, NULL, "<reset>", "soft-reset the board");
    x3switch_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "x3switch", .help = "Show x3-switch version and full status, or reset it",
        .func = &cmd_x3switch, .argtable = &x3switch_args,
    }));
}
