#include <stdio.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* One screen for both value editors: the title, the number and the gauge
   (with six step ticks for screen time) switch with the mode */
static struct {
    lv_obj_t *scr;
    lv_obj_t *bright_layer, *time_layer;
    gauge_t *bright_gauge, *time_gauge;
    lv_obj_t *title;
    lv_obj_t *bright_row, *bright_num;
    lv_obj_t *time_box, *time_num;
    lv_obj_t *wifi;
} s;

static lv_obj_t *layer_new(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, UIG_W, UIG_H);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    return o;
}

lv_obj_t *screen_edit_value_create(void)
{
    s.scr = ui_screen_create();

    const gauge_cfg_t bright = { 0 };
    s.bright_layer = layer_new(s.scr);
    s.bright_gauge = gauge_create(s.bright_layer, &bright);

    const gauge_cfg_t time = {
        .tick_count = APP_SCREEN_TIME_STEPS,
        .tick_r_in = UIG_TIME_TICK_R_IN, .tick_r_out_major = UIG_TIME_TICK_R_OUT,
        .tick_on_color = TH_ACCENT, .tick_on_opa_pct = 100, .tick_off_opa_pct = 40,
        .tick_w = UIG_TIME_TICK_W,
    };
    s.time_layer = layer_new(s.scr);
    s.time_gauge = gauge_create(s.time_layer, &time);

    lv_obj_t *stack = ui_box(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(stack, UIG_W, UIG_H);

    s.title = ui_label(stack, &font_pret500_20, TH_DIM, UIG_EDIT_TITLE_LS);
    lv_obj_set_style_pad_bottom(s.title, UIG_EDIT_TITLE_MB, 0);

    /* "70" with a small "%" sitting on its baseline */
    s.bright_row = ui_box(stack, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s.bright_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s.bright_row, UIG_EDIT_PCT_GAP, 0);
    s.bright_num = ui_tight_label_dy(s.bright_row, &font_inter200_72, TH_FG, UIG_EDIT_BRIGHT_LS,
                                    UIG_EDIT_BRIGHT_H, UIG_EDIT_NUM_DY);
    lv_obj_t *pct = ui_label(s.bright_row, &font_inter200_24, TH_DIM, 0);
    lv_obj_set_style_pad_top(pct, UIG_EDIT_PCT_DY, 0);
    lv_label_set_text(pct, "%");

    s.time_num = ui_tight_label_dy(stack, &font_inter200_64, TH_FG, UIG_EDIT_TIME_LS,
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

    ui_show(s.bright_layer, bright);
    ui_show(s.bright_row, bright);
    ui_show(s.time_layer, !bright);
    ui_show(s.time_box, !bright);

    if (bright) {
        lv_label_set_text(s.title, "화면밝기");
        snprintf(buf, sizeof(buf), "%d", st->brightness);
        ui_tight_label_set_text(s.bright_num, buf);
        gauge_set_value(s.bright_gauge, true, (st->brightness - 10) * 1000 / 90);
    } else {
        lv_label_set_text(s.title, "화면시간");
        if (st->screen_time_s >= 60) snprintf(buf, sizeof(buf), "%d분", st->screen_time_s / 60);
        else snprintf(buf, sizeof(buf), "%d초", st->screen_time_s);
        ui_tight_label_set_text(s.time_num, buf);
        gauge_set_value(s.time_gauge, true, time_index(st->screen_time_s) * 1000 / (APP_SCREEN_TIME_STEPS - 1));
    }
    wifi_set_level(s.wifi, st->wifi_level);
}
