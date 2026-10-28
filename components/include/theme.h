#pragma once
#include "lvgl.h"

/* Design tokens (design/README.md "Design Tokens"). Macros, not variables:
   lv_color_hex is not a constant expression. */
#define TH_BG            lv_color_hex(0x0E0E10)
#define TH_BG2           lv_color_hex(0x1A1A1D)
#define TH_FG            lv_color_hex(0xF5F3EE)
#define TH_DIM           lv_color_hex(0x7A7A80)
#define TH_ACCENT        lv_color_hex(0xE88A5E)
#define TH_FLAME_INNER   lv_color_hex(0xFFD27A)
#define TH_CURRENT       lv_color_hex(0x7FC8D4)
#define TH_SETTING       lv_color_hex(0xCFCFCF)
#define TH_GRAY_ON       lv_color_hex(0xB8B8BC)
#define TH_GRAY_OFF      lv_color_hex(0x4A4A50)
#define TH_BADGE_ON      lv_color_hex(0xD8D6D0)
#define TH_BADGE_OFF     lv_color_hex(0x5A5A60)

#define TH_OPA_PCT(p)    ((lv_opa_t)((p) * 255 / 100))

LV_FONT_DECLARE(font_pret500_12)
LV_FONT_DECLARE(font_pret500_13)
LV_FONT_DECLARE(font_pret500_14)
LV_FONT_DECLARE(font_pret500_15)
LV_FONT_DECLARE(font_pret500_17)
LV_FONT_DECLARE(font_pret500_18)
LV_FONT_DECLARE(font_pret500_20)
LV_FONT_DECLARE(font_pret500_64)
LV_FONT_DECLARE(font_pret400_17)
LV_FONT_DECLARE(font_pret400_23)
LV_FONT_DECLARE(font_pret400_26)
LV_FONT_DECLARE(font_pret600_23)
LV_FONT_DECLARE(font_pret600_26)
LV_FONT_DECLARE(font_pret600_32)
LV_FONT_DECLARE(font_inter200_24)
LV_FONT_DECLARE(font_inter200_28)
LV_FONT_DECLARE(font_inter200_60)
LV_FONT_DECLARE(font_inter200_64)
LV_FONT_DECLARE(font_inter200_72)
LV_FONT_DECLARE(font_jb400_17)
LV_FONT_DECLARE(font_jb400_9)

LV_IMAGE_DECLARE(img_flame_color_14);
LV_IMAGE_DECLARE(img_flame_color_10);
LV_IMAGE_DECLARE(img_flame_mono_10);
LV_IMAGE_DECLARE(img_flame_mono_9);
LV_IMAGE_DECLARE(img_wifi_0);
LV_IMAGE_DECLARE(img_wifi_1);
LV_IMAGE_DECLARE(img_wifi_2);
LV_IMAGE_DECLARE(img_wifi_3);
