#pragma once
#include <stddef.h>
#include "lvgl.h"
#include "app_state.h"

/* ui_util.c — transparent boxes and labels without theme styling */
lv_obj_t *ui_box(lv_obj_t *parent, lv_flex_flow_t flow);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int letter_space);
/* A label inside a box of fixed height, centred, overflow visible — what CSS
   line-height: 1 does. Returns the label; the box is its parent. */
lv_obj_t *ui_tight_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                         int letter_space, int box_h);
/* Text change for a ui_tight_label: the box takes the label's new width */
void ui_tight_label_set_text(lv_obj_t *label, const char *text);
/* "-12.5" style text from a ×10 value */
void ui_fmt_x10(char *buf, size_t len, int v_x10);
void ui_show(lv_obj_t *obj, bool show);

/* widget_gauge.c — arc track + value arc + ticks + current marker */
typedef struct {
    uint8_t tick_count;             /* 0 = none */
    uint8_t tick_major_every;       /* 0 = all ticks major */
    int16_t tick_r_in, tick_r_out_major, tick_r_out_minor;
    lv_color_t tick_on_color;
    uint8_t tick_on_opa_pct, tick_off_opa_pct;
    bool marker;
} gauge_cfg_t;
typedef struct gauge gauge_t;
gauge_t *gauge_create(lv_obj_t *parent, const gauge_cfg_t *cfg);
/* t_pm = 0..1000 along the sweep. show=false hides the value arc and dims all ticks */
void gauge_set_value(gauge_t *g, bool show, int t_pm);
void gauge_set_marker(gauge_t *g, bool show, int t_pm);

/* widget_flame.c */
typedef enum { FLAME_COLOR_14, FLAME_COLOR_10, FLAME_MONO_10, FLAME_MONO_9 } flame_kind_t;
lv_obj_t *flame_create(lv_obj_t *parent, flame_kind_t kind);
void flame_set_color(lv_obj_t *flame, lv_color_t c);     /* mono kinds only */

/* widget_wifi.c */
lv_obj_t *wifi_create(lv_obj_t *parent);
void wifi_set_level(lv_obj_t *wifi, uint8_t level);

/* screen_main.c */
lv_obj_t *screen_main_create(void);
void screen_main_refresh(void);

/* ui_screens.c — create all screens, load the one for the mode, refresh all */
void ui_screens_create(void);
void ui_screens_refresh(void);
void ui_screens_set_anim(bool on);
bool ui_screens_anim(void);

/* ui_test.c — docs/ui.md §6.2. out() receives the reply in pieces
   (SNAP is ~150 KB); the reply ends with "\n". */
typedef void (*ui_test_out_t)(const char *s, size_t len, void *arg);
void ui_test_exec(const char *line, ui_test_out_t out, void *arg);
