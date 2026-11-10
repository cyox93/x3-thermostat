#include <stdio.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* One screen for both value editors: the title and the number switch with
   the mode; the gauge is the same for both */
static struct {
    lv_obj_t *scr;
    gauge_t *gauge;
    lv_obj_t *title;
    lv_obj_t *bright_row, *bright_num;
    lv_obj_t *time_box, *time_num;
    lv_obj_t *wifi;
} s;

lv_obj_t *screen_edit_value_create(void)
{
    s.scr = ui_screen_create();

    s.gauge = gauge_create(s.scr, false);

    lv_obj_t *stack = ui_box(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(stack, UIG_W, UIG_H);

    s.title = ui_label(stack, &font_pret500_20, TH_DIM, UIG_EDIT_TITLE_LS);
    lv_obj_set_style_pad_bottom(s.title, UIG_EDIT_TITLE_MB, 0);

    /* "70" with a small "%" sitting on its baseline */
    s.bright_row = ui_box(stack, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s.bright_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s.bright_row, UIG_EDIT_PCT_GAP, 0);
    s.bright_num = ui_tight_label_dy(s.bright_row, &font_inter300_72, TH_FG, UIG_EDIT_BRIGHT_LS,
                                    UIG_EDIT_BRIGHT_H, UIG_EDIT_NUM_DY);
    lv_obj_t *pct = ui_label(s.bright_row, &font_inter300_24, TH_DIM, 0);
    lv_obj_set_style_pad_top(pct, UIG_EDIT_PCT_DY, 0);
    lv_label_set_text(pct, "%");

    s.time_num = ui_tight_label_dy(stack, &font_inter300_64, TH_FG, UIG_EDIT_TIME_LS,
                                   UIG_EDIT_TIME_H, UIG_EDIT_NUM_DY);
    s.time_box = lv_obj_get_parent(s.time_num);

    lv_obj_t *hint = ui_label(stack, &font_pret500_13, TH_DIM, UIG_EDIT_HINT_LS);
    lv_obj_set_style_opa(hint, TH_OPA_PCT(UIG_EDIT_HINT_OPA_PCT), 0);
    lv_obj_set_style_pad_top(hint, UIG_EDIT_HINT_MT, 0);
    lv_label_set_text(hint, "돌려 조절 · 눌러 확인");

    s.wifi = wifi_create(s.scr);
    return s.scr;
}

static int time_index(uint16_t sec)
{
    for (int i = 0; i < APP_SCREEN_TIME_STEPS; i++) {
        if (app_screen_time_steps[i] == sec) return i;
    }
    return 0;
}

void screen_edit_value_refresh(void)
{
    if (lv_screen_active() != s.scr) return;
    const app_state_t *st = app_state_get();
    bool bright = st->mode == APP_MODE_EDIT_BRIGHTNESS;
    char buf[16];

    ui_show(s.bright_row, bright);
    ui_show(s.time_box, !bright);

    if (bright) {
        lv_label_set_text(s.title, "화면밝기");
        snprintf(buf, sizeof(buf), "%d", st->brightness);
        ui_tight_label_set_text(s.bright_num, buf);
        gauge_set_value(s.gauge, true, (st->brightness - 10) * 1000 / 90);
    } else {
        lv_label_set_text(s.title, "화면시간");
        ui_tight_label_set_text(s.time_num, app_screen_time_label(st->screen_time_s, buf, sizeof(buf)));
        gauge_set_value(s.gauge, true, time_index(st->screen_time_s) * 1000 / (APP_SCREEN_TIME_STEPS - 1));
    }
    wifi_set_level(s.wifi, st->wifi_level);
}
