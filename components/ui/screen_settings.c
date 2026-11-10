#include <stdio.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

#define ITEMS 4

static struct {
    lv_obj_t *scr;
    focus_list_t *list;
    lv_obj_t *name[ITEMS], *value[ITEMS];
    lv_obj_t *wifi;
} s;

lv_obj_t *screen_settings_create(void)
{
    static const char *const names[ITEMS] = { "방선택", "화면밝기", "화면시간", "이전" };
    s.scr = ui_screen_create();
    header_create(s.scr, "설정");

    s.list = focus_list_create(s.scr, ITEMS, UIG_SET_ITEM_H);
    for (int i = 0; i < ITEMS; i++) {
        lv_obj_t *row = focus_list_row(s.list, i);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(row, UIG_SET_PAD_X, 0);
        s.name[i] = ui_label(row, &font_pret400_23, TH_FG, 0);
        lv_label_set_text(s.name[i], names[i]);
        s.value[i] = ui_label(row, &font_jb400_17, TH_DIM, 0);
    }
    s.wifi = wifi_create(s.scr);
    return s.scr;
}

void screen_settings_refresh(void)
{
    if (lv_screen_active() != s.scr) {
        focus_list_reset(s.list);
        return;
    }
    const app_state_t *st = app_state_get();
    char buf[16];

    lv_label_set_text(s.value[0], app_room_name(st->installed_room));
    lv_label_set_text_fmt(s.value[1], "%d%%", st->brightness);
    lv_label_set_text(s.value[2], app_screen_time_label(st->screen_time_s, buf, sizeof(buf)));
    lv_label_set_text(s.value[3], "");

    for (int i = 0; i < ITEMS; i++) {
        bool active = i == st->settings_sel;
        lv_obj_set_style_text_font(s.name[i], active ? &font_pret600_23 : &font_pret400_23, 0);
        lv_obj_set_style_text_color(s.value[i], active ? TH_ACCENT : TH_DIM, 0);
    }
    focus_list_select(s.list, st->settings_sel, ui_screens_anim());
    wifi_set_level(s.wifi, st->wifi_level);
}
