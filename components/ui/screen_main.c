#include <stdio.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

static struct {
    lv_obj_t *scr;
    gauge_t *gauge;
    lv_obj_t *flame_lit, *flame_idle, *room;
    lv_obj_t *cur_row, *cur_temp, *cur_hum;
    lv_obj_t *hero_row, *hero, *off;
    lv_obj_t *wifi;
    lv_obj_t *stack, *badge, *summary;
} s;

static int target_t_pm(int x10)
{
    return (x10 - APP_TARGET_MIN_X10) * 1000 / (APP_TARGET_MAX_X10 - APP_TARGET_MIN_X10);
}

/* qa-flame on the colour flame before the room name: same keyframes as the badge */
static void flicker_cb(void *img, int32_t t)
{
    static const int16_t key_t[] = { 0, 420, 840, UIG_FLAME_ANIM_MS };
    static const int16_t key_x[] = { 256, 236, 269, 256 };
    static const int16_t key_y[] = { 256, 276, 241, 256 };
    int k = t < key_t[1] ? 0 : t < key_t[2] ? 1 : 2;
    int span = key_t[k + 1] - key_t[k], d = t - key_t[k];
    lv_image_set_scale_x(img, key_x[k] + (key_x[k + 1] - key_x[k]) * d / span);
    lv_image_set_scale_y(img, key_y[k] + (key_y[k + 1] - key_y[k]) * d / span);
}

static void flicker(lv_obj_t *img, bool on)
{
    if (on == (lv_anim_get(img, flicker_cb) != NULL)) return;
    lv_anim_delete(img, flicker_cb);
    lv_image_set_scale_x(img, LV_SCALE_NONE);
    lv_image_set_scale_y(img, LV_SCALE_NONE);
    if (!on) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, flicker_cb);
    lv_anim_set_values(&a, 0, UIG_FLAME_ANIM_MS);
    lv_anim_set_duration(&a, UIG_FLAME_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

lv_obj_t *screen_main_create(void)
{
    s.scr = ui_screen_create();
    s.gauge = gauge_create(s.scr, true);

    /* Centre stack: a full-screen column centred both ways, like the design's flex box */
    lv_obj_t *stack = ui_box(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(stack, UIG_W, UIG_H);
    s.stack = stack;

    /* [flame] room — the flame is always there: colour and flickering while
       this room burns, grey and still otherwise */
    lv_obj_t *room_row = ui_box(stack, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(room_row, UIG_MAIN_ROOM_FLAME_GAP, 0);
    lv_obj_set_style_pad_bottom(room_row, UIG_MAIN_ROOM_MB, 0);
    s.flame_lit = flame_create(room_row, FLAME_COLOR_14);
    lv_image_set_pivot(s.flame_lit, img_flame_color_14.header.w / 2, img_flame_color_14.header.h);
    s.flame_idle = flame_create(room_row, FLAME_MONO_14);
    flame_set_color(s.flame_idle, TH_FLAME_IDLE);
    s.room = ui_label(room_row, &font_pret500_20, TH_DIM, UIG_MAIN_ROOM_LS);

    s.cur_row = ui_box(stack, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s.cur_row, UIG_MAIN_CUR_GAP, 0);
    lv_obj_set_style_pad_bottom(s.cur_row, UIG_MAIN_CUR_MB, 0);
    s.cur_temp = ui_label(s.cur_row, &font_pret500_21, TH_DIM, UIG_MAIN_CUR_LS);
    s.cur_hum = ui_label(s.cur_row, &font_pret500_21, TH_DIM, UIG_MAIN_CUR_LS);

    /* Fixed-height slot: ON/OFF swaps only its content, so nothing else moves */
    lv_obj_t *slot = lv_obj_create(stack);
    lv_obj_remove_style_all(slot);
    lv_obj_set_size(slot, UIG_W, UIG_MAIN_SLOT_H);
    lv_obj_set_style_margin_top(slot, UIG_MAIN_SLOT_MT, 0);
    lv_obj_set_scrollable(slot, false);
    lv_obj_set_clickable(slot, false);

    /* on: hero number with ° at its top edge */
    s.hero_row = ui_box(slot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s.hero_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s.hero_row, UIG_MAIN_DEG_GAP, 0);
    lv_obj_center(s.hero_row);
    s.hero = ui_tight_label(s.hero_row, &font_inter300_64, TH_FG, UIG_MAIN_HERO_LS, UIG_MAIN_HERO_H);
    lv_obj_t *deg = ui_tight_label(s.hero_row, &font_inter300_28, TH_DIM, 0, UIG_MAIN_DEG_H);
    ui_tight_label_set_text(deg, "°");

    /* off: OFF only */
    s.off = ui_tight_label(slot, &font_inter300_60, TH_DIM, UIG_MAIN_OFF_LS, UIG_MAIN_OFF_H);
    ui_tight_label_set_text(s.off, "OFF");
    lv_obj_center(lv_obj_get_parent(s.off));

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

    /* No reading at all (sensor missing) shows "--" so the line never just
       vanishes; a temperature without humidity drops the humidity, as the
       design does */
    bool has_cur = r->current_x10 != APP_TEMP_NONE;
    if (has_cur) {
        char t[12];
        ui_fmt_x10(t, sizeof(t), r->current_x10);
        snprintf(buf, sizeof(buf), "현재 %s°", t);
        lv_label_set_text(s.cur_temp, buf);
    } else {
        lv_label_set_text(s.cur_temp, "현재 --°");
    }
    bool has_hum = r->humidity != APP_HUM_NONE;
    ui_show(s.cur_hum, has_hum || !has_cur);
    if (has_hum) lv_label_set_text_fmt(s.cur_hum, "%d%%", r->humidity);
    else lv_label_set_text(s.cur_hum, "--%");

    ui_show(s.hero_row, r->on);
    ui_show(lv_obj_get_parent(s.off), !r->on);
    if (r->on) {
        ui_fmt_x10(buf, sizeof(buf), r->target_x10);
        ui_tight_label_set_text(s.hero, buf);
        lv_obj_center(s.hero_row);
    }
    /* Colour flame only while this room's heating is on and the boiler side
       reports it burning */
    bool heating = r->on && r->burning;
    ui_show(s.flame_lit, heating);
    ui_show(s.flame_idle, !heating);
    flicker(s.flame_lit, heating && ui_screens_anim());

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
