#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

static struct {
    lv_obj_t *scr;
    focus_list_t *list;
    lv_obj_t *name[APP_ROOM_COUNT];
    lv_obj_t *wifi;
} s;

lv_obj_t *screen_edit_room_create(void)
{
    s.scr = ui_screen_create();
    header_create(s.scr, "방 선택");

    s.list = focus_list_create(s.scr, APP_ROOM_COUNT, UIG_ROOM_ITEM_H);
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        lv_obj_t *row = focus_list_row(s.list, i);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        s.name[i] = ui_label(row, &font_pret400_26, TH_FG, 0);
        lv_label_set_text(s.name[i], app_room_name(i));
    }

    lv_obj_t *hint = ui_label(s.scr, &font_pret500_17, TH_DIM, UIG_ROOM_HINT_LS);
    lv_label_set_text(hint, "돌려 선택 · 눌러 확인");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -UIG_ROOM_HINT_BOTTOM);

    s.wifi = wifi_create(s.scr);
    return s.scr;
}

void screen_edit_room_refresh(void)
{
    if (lv_screen_active() != s.scr) {
        focus_list_reset(s.list);
        return;
    }
    const app_state_t *st = app_state_get();
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        bool active = i == (int)st->room_edit_sel;
        lv_obj_set_style_text_font(s.name[i], active ? &font_pret600_26 : &font_pret400_26, 0);
        lv_obj_set_style_text_color(s.name[i], active ? TH_ACCENT : TH_FG, 0);
    }
    focus_list_select(s.list, st->room_edit_sel, ui_screens_anim());
    wifi_set_level(s.wifi, st->wifi_level);
}
