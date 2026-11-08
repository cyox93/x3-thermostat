#pragma once
#include <stddef.h>
#include "lvgl.h"
#include "app_state.h"
#include "ui.h"

/* ui_util.c — transparent boxes and labels without theme styling */
lv_obj_t *ui_box(lv_obj_t *parent, lv_flex_flow_t flow);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int letter_space);
/* A label inside a box of fixed height, vertically centred, overflow visible —
   what CSS line-height: 1 does. Returns the label; the box is its parent.
   Set its text with ui_tight_label_set_text. */
lv_obj_t *ui_tight_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                         int letter_space, int box_h);
/* Same with an explicit vertical nudge instead of UIG_TIGHT_DY (tuned for the big numbers) */
lv_obj_t *ui_tight_label_dy(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                            int letter_space, int box_h, int dy);
/* Text change for a ui_tight_label: the box takes the label's new width */
void ui_tight_label_set_text(lv_obj_t *label, const char *text);
/* "-12.5" style text from a ×10 value */
void ui_fmt_x10(char *buf, size_t len, int v_x10);
void ui_show(lv_obj_t *obj, bool show);

/* widget_gauge.c — arc track + value arc + optional current marker */
typedef struct gauge gauge_t;
gauge_t *gauge_create(lv_obj_t *parent, bool marker);
/* t_pm = 0..1000 along the sweep. show=false hides the value arc */
void gauge_set_value(gauge_t *g, bool show, int t_pm);
void gauge_set_marker(gauge_t *g, bool show, int t_pm);

/* widget_flame.c */
typedef enum { FLAME_COLOR_14, FLAME_COLOR_10, FLAME_MONO_9 } flame_kind_t;
lv_obj_t *flame_create(lv_obj_t *parent, flame_kind_t kind);
void flame_set_color(lv_obj_t *flame, lv_color_t c);     /* mono kinds only */

/* widget_wifi.c */
lv_obj_t *wifi_create(lv_obj_t *parent);
void wifi_set_level(lv_obj_t *wifi, uint8_t level);

/* widget_focus_list.c — the design's focus-centred lists + the list header */
typedef struct focus_list focus_list_t;
focus_list_t *focus_list_create(lv_obj_t *parent, int count, int item_h);
lv_obj_t *focus_list_row(focus_list_t *l, int i);
void focus_list_select(focus_list_t *l, int sel, bool animate);
void focus_list_reset(focus_list_t *l);
lv_obj_t *header_create(lv_obj_t *parent, const char *text);

/* widget_boiler_badge.c / widget_room_summary.c — master-only parts of MAIN */
lv_obj_t *badge_create(lv_obj_t *parent);
void badge_set(lv_obj_t *badge, bool burning);
lv_obj_t *summary_create(lv_obj_t *parent);
void summary_set(lv_obj_t *summary, const app_state_t *st);

/* Screens: create once, refresh draws app_state (no-op unless active) */
lv_obj_t *ui_screen_create(void);
lv_obj_t *screen_main_create(void);
void screen_main_refresh(void);
lv_obj_t *screen_settings_create(void);
void screen_settings_refresh(void);
lv_obj_t *screen_edit_room_create(void);
void screen_edit_room_refresh(void);
lv_obj_t *screen_edit_value_create(void);      /* brightness and screen time */
void screen_edit_value_refresh(void);

/* ui_screens.c — create all screens, load the one for the mode, refresh all */
void ui_screens_create(void);
void ui_screens_refresh(void);
void ui_screens_set_anim(bool on);
bool ui_screens_anim(void);

/* ui_input.c — one input through the state machine, then the press flash.
   UI thread only; the device and the sim feed it the same way. */
void ui_input_apply(ui_input_t in);

/* widget_flash.c — the press flash on lv_layer_top */
void flash_init(void);
void flash_show(const char *text, lv_color_t accent);

/* ui_test.c — docs/ui.md §6.2. out() receives the reply in pieces
   (SNAP is ~150 KB); the reply ends with "\n". */
typedef void (*ui_test_out_t)(const char *s, size_t len, void *arg);
void ui_test_exec(const char *line, ui_test_out_t out, void *arg);
