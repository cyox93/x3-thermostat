#pragma once
#include "lvgl.h"

/* Design tokens (design/README.md "Design Tokens"). Macros, not variables:
   lv_color_hex is not a constant expression. */
#define TH_BG            lv_color_hex(0x0E0E10)
#define TH_BG2           lv_color_hex(0x4A4A52)
#define TH_FG            lv_color_hex(0xFFFFFF)
#define TH_DIM           lv_color_hex(0xD0D0D4)
#define TH_ACCENT        lv_color_hex(0xE88A5E)
#define TH_FLAME_INNER   lv_color_hex(0xFFD27A)
#define TH_FLAME_IDLE    lv_color_hex(0x6E6E75)     /* flame before the room name, not burning */
#define TH_CURRENT       lv_color_hex(0x7FC8D4)
#define TH_SETTING       lv_color_hex(0xCFCFCF)
#define TH_GRAY_ON       lv_color_hex(0xFFFFFF)
#define TH_GRAY_OFF      lv_color_hex(0x9A9AA0)
#define TH_BADGE_ON      lv_color_hex(0xFFFFFF)
#define TH_BADGE_OFF     lv_color_hex(0xA8A8AE)
#define TH_BADGE_OFF_FLAME lv_color_hex(0x8E8E95)

#define TH_OPA_PCT(p)    ((lv_opa_t)((p) * 255 / 100))

LV_FONT_DECLARE(font_pret500_13)
LV_FONT_DECLARE(font_pret500_17)
LV_FONT_DECLARE(font_pret500_18)
LV_FONT_DECLARE(font_pret500_20)
LV_FONT_DECLARE(font_pret500_21)
LV_FONT_DECLARE(font_pret500_64)
LV_FONT_DECLARE(font_pret400_17)
LV_FONT_DECLARE(font_pret400_23)
LV_FONT_DECLARE(font_pret400_26)
LV_FONT_DECLARE(font_pret600_12)
LV_FONT_DECLARE(font_pret600_23)
LV_FONT_DECLARE(font_pret600_26)
LV_FONT_DECLARE(font_pret600_32)
LV_FONT_DECLARE(font_inter300_24)
LV_FONT_DECLARE(font_inter300_28)
LV_FONT_DECLARE(font_inter300_60)
LV_FONT_DECLARE(font_inter300_64)
LV_FONT_DECLARE(font_inter300_72)
LV_FONT_DECLARE(font_jb400_17)
LV_FONT_DECLARE(font_jb400_9)

LV_IMAGE_DECLARE(img_flame_color_14);
LV_IMAGE_DECLARE(img_flame_color_10);
LV_IMAGE_DECLARE(img_flame_mono_14);
LV_IMAGE_DECLARE(img_flame_mono_10);
LV_IMAGE_DECLARE(img_flame_mono_9);
LV_IMAGE_DECLARE(img_wifi_0);
LV_IMAGE_DECLARE(img_wifi_1);
LV_IMAGE_DECLARE(img_wifi_2);
LV_IMAGE_DECLARE(img_wifi_3);
