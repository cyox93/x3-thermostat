#include <stdio.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

static struct {
    lv_obj_t *scr;
    gauge_t *gauge;
    lv_obj_t *room;
    lv_obj_t *cur_row, *cur_temp, *cur_hum;
    lv_obj_t *on_box, *hero, *flame;
    lv_obj_t *off_box;
    lv_obj_t *wifi;
    lv_obj_t *stack, *badge, *summary;
} s;

static int target_t_pm(int x10)
{
    return (x10 - APP_TARGET_MIN_X10) * 1000 / (APP_TARGET_MAX_X10 - APP_TARGET_MIN_X10);
}

lv_obj_t *screen_main_create(void)
{
    s.scr = lv_obj_create(NULL);
    lv_obj_set_scrollable(s.scr, false);
    lv_obj_set_style_bg_color(s.scr, TH_BG, 0);
    lv_obj_set_style_bg_opa(s.scr, LV_OPA_COVER, 0);

    const gauge_cfg_t gcfg = {
        .tick_count = UIG_TICK_COUNT, .tick_major_every = UIG_TICK_MAJOR_EVERY,
        .tick_r_in = UIG_TICK_R_IN, .tick_r_out_major = UIG_TICK_R_OUT_MAJOR,
        .tick_r_out_minor = UIG_TICK_R_OUT_MINOR,
        .tick_on_color = TH_FG, .tick_on_opa_pct = 85, .tick_off_opa_pct = 30,
        .marker = true,
    };
    s.gauge = gauge_create(s.scr, &gcfg);

    /* Centre stack: a full-screen column centred both ways, like the design's flex box */
    lv_obj_t *stack = ui_box(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(stack, UIG_W, UIG_H);
    s.stack = stack;

    s.room = ui_label(stack, &font_pret500_20, TH_DIM, UIG_MAIN_ROOM_LS);
    lv_obj_set_style_pad_bottom(s.room, UIG_MAIN_ROOM_MB, 0);

    s.cur_row = ui_box(stack, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s.cur_row, UIG_MAIN_CUR_GAP, 0);
    lv_obj_set_style_pad_bottom(s.cur_row, UIG_MAIN_CUR_MB, 0);
    s.cur_temp = ui_label(s.cur_row, &font_pret400_17, TH_DIM, UIG_MAIN_CUR_LS);
    s.cur_hum = ui_label(s.cur_row, &font_pret400_17, TH_DIM, UIG_MAIN_CUR_LS);

    /* on: hero number with ° at its top edge, then the 희망온도 label */
    s.on_box = ui_box(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *hero_row = ui_box(s.on_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hero_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(hero_row, UIG_MAIN_DEG_GAP, 0);
    lv_obj_set_style_pad_top(hero_row, UIG_MAIN_HERO_MT, 0);
    s.hero = ui_tight_label(hero_row, &font_inter200_64, TH_FG, UIG_MAIN_HERO_LS, UIG_MAIN_HERO_H);
    lv_obj_t *deg = ui_tight_label(hero_row, &font_inter200_28, TH_DIM, 0, UIG_MAIN_DEG_H);
    ui_tight_label_set_text(deg, "°");

    lv_obj_t *label_row = ui_box(s.on_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(label_row, UIG_MAIN_LABEL_GAP, 0);
    lv_obj_set_style_pad_top(label_row, UIG_MAIN_LABEL_MT, 0);
    s.flame = flame_create(label_row, FLAME_COLOR_14);
    lv_obj_t *label = ui_label(label_row, &font_pret500_15, TH_ACCENT, UIG_MAIN_LABEL_LS);
    lv_label_set_text(label, "희망온도");

    /* off: OFF + hint */
    s.off_box = ui_box(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *off_wrap = ui_box(s.off_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(off_wrap, UIG_MAIN_OFF_MT, 0);
    lv_obj_t *off = ui_tight_label(off_wrap, &font_inter200_60, TH_DIM, UIG_MAIN_OFF_LS, UIG_MAIN_OFF_H);
    ui_tight_label_set_text(off, "OFF");
    lv_obj_t *hint = ui_label(s.off_box, &font_pret500_14, TH_DIM, 0);
    lv_obj_set_style_pad_top(hint, UIG_MAIN_HINT_MT, 0);
    lv_label_set_text(hint, "버튼을 눌러 켜기");

    s.badge = badge_create(s.scr);
    s.summary = summary_create(s.scr);
    s.wifi = wifi_create(s.scr);
    return s.scr;
}

void screen_main_refresh(void)
{
    if (lv_screen_active() != s.scr) return;
    const app_state_t *st = app_state_get();
    const app_room_state_t *r = &st->rooms[st->active_room];
    char buf[24];

    lv_label_set_text(s.room, app_room_name(st->active_room));

    bool has_cur = r->current_x10 != APP_TEMP_NONE;
    ui_show(s.cur_row, has_cur);
    if (has_cur) {
        char t[12];
        ui_fmt_x10(t, sizeof(t), r->current_x10);
        snprintf(buf, sizeof(buf), "현재 %s°", t);
        lv_label_set_text(s.cur_temp, buf);
    }
    ui_show(s.cur_hum, r->humidity != APP_HUM_NONE);
    if (r->humidity != APP_HUM_NONE) lv_label_set_text_fmt(s.cur_hum, "%d%%", r->humidity);

    ui_show(s.on_box, r->on);
    ui_show(s.off_box, !r->on);
    if (r->on) {
        ui_fmt_x10(buf, sizeof(buf), r->target_x10);
        ui_tight_label_set_text(s.hero, buf);
    }
    /* Flame only when the boiler side reports this room burning */
    ui_show(s.flame, r->on && r->burning);

    gauge_set_value(s.gauge, r->on, target_t_pm(r->target_x10));
    gauge_set_marker(s.gauge, has_cur, has_cur ? target_t_pm(r->current_x10) : 0);
    wifi_set_level(s.wifi, st->wifi_level);

    /* Master only: the boiler badge and the four-room summary */
    bool master = app_state_is_master();
    bool any_burning = false;
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        any_burning |= st->rooms[i].on && st->rooms[i].burning;
    }
    lv_obj_set_style_pad_top(s.stack, master ? UIG_MASTER_STACK_PT : 0, 0);
    lv_obj_set_style_pad_bottom(s.stack, master ? UIG_MASTER_STACK_PB : 0, 0);
    ui_show(s.badge, master);
    ui_show(s.summary, master);
    if (master) {
        badge_set(s.badge, any_burning);
        summary_set(s.summary, st);
    }
}
