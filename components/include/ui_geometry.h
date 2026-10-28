#pragma once
/* Every coordinate, radius, angle and spacing the screens use — mirrors
   design/src/boiler-app.jsx (240×240, centre 120,120). Screens never
   hard-code numbers; change them here when tuning against the golden diff.
   Angles are design angles: 12 o'clock = 0°, clockwise positive. */

#define UIG_W                 240
#define UIG_H                 240
#define UIG_CX                120
#define UIG_CY                120

/* Gauge (ScreenBoiler / ScreenBrightness / ScreenScreenTime) */
#define UIG_ARC_START_DEG     (-130)
#define UIG_ARC_SWEEP_DEG     260
#define UIG_ARC_R             100     /* stroke centre line */
#define UIG_ARC_TRACK_W       2
#define UIG_ARC_VALUE_W       3       /* design 2.5 */

#define UIG_TICK_COUNT        16
#define UIG_TICK_MAJOR_EVERY  3
#define UIG_TICK_R_IN         108
#define UIG_TICK_R_OUT_MAJOR  116
#define UIG_TICK_R_OUT_MINOR  112
#define UIG_TICK_W            1       /* major 1, minor 0.6 → width 1 at reduced opacity */
#define UIG_TICK_MINOR_OPA_PCT 60

#define UIG_CUR_R_IN          105
#define UIG_CUR_R_OUT         122
#define UIG_CUR_W             3       /* design 2.5 */

/* Labels with CSS line-height: 1 (ui_tight_label): CSS places the glyph
   −6.5 px into the box for Inter 64, LVGL rounds the centring the other
   way. Measured against the golden frames. */
#define UIG_TIGHT_DY          (-1)

/* MAIN centre stack — margins are CSS marginTop/Bottom, letter spacing is em × size */
#define UIG_MAIN_ROOM_MB      5
#define UIG_MAIN_ROOM_LS      3       /* 0.13em × 20 */
#define UIG_MAIN_CUR_MB       3
#define UIG_MAIN_CUR_GAP      8
#define UIG_MAIN_CUR_LS       1       /* 0.04em × 17 */
#define UIG_MAIN_HERO_MT      2
#define UIG_MAIN_HERO_H       64      /* line-height 1 */
#define UIG_MAIN_HERO_LS      (-2)    /* −0.04em × 64 = −2.56; −3 measured 1 px/glyph too tight */
#define UIG_MAIN_DEG_GAP      2       /* design 4; LVGL rounds each glyph advance, so the number box comes out ~2 px wider */
#define UIG_MAIN_DEG_H        28
#define UIG_MAIN_LABEL_MT     4
#define UIG_MAIN_LABEL_GAP    5
#define UIG_MAIN_LABEL_LS     1       /* 0.04em × 15 */
#define UIG_MAIN_OFF_MT       4
#define UIG_MAIN_OFF_H        60
#define UIG_MAIN_OFF_LS       (-2)    /* −0.04em × 60 = −2.4 */
#define UIG_MAIN_HINT_MT      6

/* WiFi status bar */
#define UIG_WIFI_BOTTOM       14
#define UIG_WIFI_TEXT_GAP     3
#define UIG_WIFI_LS           1       /* 0.08em × 9 */
