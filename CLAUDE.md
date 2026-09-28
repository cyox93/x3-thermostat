# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP-IDF firmware (`x3-thermostat`) for the **main-board rev A — ESP32-S3-WROOM-1-N16R8**
(16 MB quad flash, 8 MB octal PSRAM). It is the brain of one X3 Thermostat wall unit:
it drives the X3 panel (GC9A01A LCD, rotary encoder, LED ring + backlight), reads an
AHT25, talks to the boiler controller over a PLC modem, links lights over WiFi/MQTT,
and commands the x3-switch board inside the X3 over UART using the AT protocol.

The x3-switch board firmware is a separate repository, `x3-switch`. **Its `docs/at-protocol.md`
is the contract; this repo is the client.** The board is a pure slave — all policy lives here.
Code here talks to it only as "x3-switch" over UART (`components/x3switch/`); never depend on
what MCU sits behind it.

Read `docs/` before changing behaviour:

| Document | Contents |
|---|---|
| `docs/architecture.md` | Role split, component layout, event flow, x3-switch link rules |
| `docs/hardware.md` | Pin map, N16R8 constraints, electrical behaviour |
| `docs/roadmap.md` | Phases and status |
| `docs/plc-protocol.md` | Boiler-bus PLC: waveform, messages, items, sequences, RMT implementation, what X3 must do |

System-level design (power distribution, PLC, board fabrication, X3 modifications) is
kept outside this repository. `docs/` carries everything the firmware needs — keep it
self-contained and do not link to external notes from repo files.

## Build

```sh
source ~/.espressif/tools/activate_idf_v6.0.sh
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

In non-interactive shells `idf.py` may be only an alias; call it as
`"$HOME/.espressif/tools/python/v6.0/venv/bin/python" "$IDF_PATH/tools/idf.py" ...`.

Config lives in `sdkconfig.defaults`; `sdkconfig` is generated. Bench-only code (the PLC
MC200 stand-in, `CONFIG_SC_PLC_MC200_STANDIN`) is off by default and must stay out of the
firmware for the real boiler bus; build it separately with
`-B build-bench -D SDKCONFIG=build-bench/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench"`.

There are no tests — this is firmware that runs on hardware. Back behavioural claims
with evidence from the bench, not with reasoning alone.

## Architecture

- **`components/include/board.h`** — the only place GPIO numbers are defined. Drivers
  use `BOARD_*` macros, never raw numbers. Keep it in sync with `docs/hardware.md`.
- **`components/core/`** — `eventloop` (esp_event + timers, `sc_eventloop_*`),
  `console` (esp_console REPL over USB Serial/JTAG), `config` (NVS string store).
- **`components/device/`** — drivers: `gc9a01` (LCD), `led` (ring + backlight),
  `encoder` (PCNT, ISR callback), `als` (ambient light, ADC1), `temp_humid` (AHT25, I2C —
  `measure_trigger` then `get`, chip hidden behind the API), `gpio_cmd` (debug). `device.c::sc_device_init()` brings them up.
- **`components/x3switch/`** — AT client for the x3-switch board; notifications via callback.
- **`components/plc/`** — PLC modem link on RMT: RX decodes one frame = one byte in the ISR,
  TX sends open-drain on `/SCTL`. Waveform rules and what is still unknown: `docs/plc-protocol.md`.
- **`main/`** — the application as *loops*, same layout as the user's `scmbd` project:
  `main.c` (init order), `main.h` (public loop APIs), `mainloop.c` (inits loops),
  `switchloop.c` (owns relays / button LEDs / keys), `sysloop.c` (owns policy, sensors,
  display brightness levels), `plcloop.c` (gathers PLC frames into groups, sends).

**Event-driven rule**: drivers never know about eventloops — they expose callbacks.
Each `xxxloop.c` owns one `sc_eventloop`, a private `_xxxloop_t` state and its
event/timer ids; callbacks only post, and state changes only in the loop thread.
Periodic work is an eventloop timer, never a polling task; a loop handler may wait a
few ms (the ALS averages over 10 ms) but never long — split longer waits into a
trigger plus a one-shot timer, as `temp_humid` does. Loops talk to each other through the other loop's API (which posts),
never by touching its state. See `docs/architecture.md` §3.

### Hardware traps

1. **GPIO 33–37 are octal PSRAM** on N16R8 — never assign them.
2. **Backlight and LED-ring enable are one net** (IO14). PWM dims the LCD; the ring is
   on/off only; duty 0 turns both off.
3. **PLC `/SCTL` (IO39) must be open-drain** — the module pulls it to +5 V.
4. **The x3-switch board has no hardware reset line.** Recovery is `AT+RST` only.
   **K4 currently resets the board** (x3-switch `docs/hardware.md` §2.1.1) — don't use K4 in policy.
5. **Ambient light sensor must stay on ADC1** (IO1). ADC2 is unusable with WiFi on,
   and the sensor must not see the unit's own LCD/LED light (feedback loop).
6. USB-C alone cannot power the X3 body (relays); only the panel works on USB.
7. **Without the ambient light sensor the screen turns off after ~20 s** (reads as dark →
   NIGHT level). On the bench run `bright normal`.

## Conventions

- **Adding a driver**: `components/device/<name>.c` (or a new component for a subsystem),
  declare in `components/include/device.h`, init from `sc_device_init()`, and register
  console commands so it can be exercised by hand. Inputs → `register_callback`.
- **Adding a loop**: `main/<name>loop.c`, API in `main/main.h`, init from
  `sc_mainloop_init()`. Outputs a loop owns are changed from the console only through
  that loop's commands, so its state never drifts from the hardware.
- Prefix public symbols with `sc_` as the existing code does.
- Docs are written in Korean; `docs/hardware.md` records measurements — keep them.
- **Commit messages** follow `x3-switch`: `{type} : {scope} : {sub-scope} : {summary}`,
  body indented 2 spaces explaining why / evidence, then the Co-Authored-By trailer.
- **Comments**: explain *why*, not *what*.
