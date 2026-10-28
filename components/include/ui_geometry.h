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

/* Press flash (PressOverlay) */
#define UIG_FLASH_MS          600
#define UIG_FLASH_IN_MS       150
#define UIG_FLASH_R           110     /* 65 % of the gradient's farthest-corner radius */
#define UIG_FLASH_DISC_OPA_PCT 10     /* mean of the 25 % → 0 falloff over the disc */

/* Master (거실) extras on MAIN */
#define UIG_MASTER_STACK_PT   14      /* the centre stack moves down to make room */
#define UIG_MASTER_STACK_PB   16
#define UIG_BADGE_TOP         27      /* design 26; measured 1 px high */
#define UIG_BADGE_GAP         4
#define UIG_SUMMARY_BOTTOM    34
#define UIG_SUMMARY_GAP       5       /* between cells */
#define UIG_SUMMARY_PAD_T     1
#define UIG_SUMMARY_PAD_R     4
#define UIG_SUMMARY_PAD_B     1
#define UIG_SUMMARY_PAD_L     2
#define UIG_SUMMARY_RADIUS    4
#define UIG_SUMMARY_FLAME_W   9       /* slot always kept so letters never move */
#define UIG_SUMMARY_FLAME_H   12
#define UIG_SUMMARY_INNER_GAP 2
#define UIG_SUMMARY_TEXT_H    13      /* line-height 1.1 × 12 */
#define UIG_SUMMARY_TEXT_DY   1       /* measured: letters sat 1.5 px high */
