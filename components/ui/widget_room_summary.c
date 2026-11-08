#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* 거 / 큰 / 일 / 노 cells at the bottom of the master's MAIN screen.
   Child layout per cell: 0 = flame slot (child 0 = flame), 1 = letter. */
lv_obj_t *summary_create(lv_obj_t *parent)
{
    static const char *const letters[APP_ROOM_COUNT] = { "거", "큰", "일", "노" };
    lv_obj_t *row = ui_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UIG_SUMMARY_GAP, 0);

    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        lv_obj_t *cell = ui_box(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(cell, UIG_SUMMARY_INNER_GAP, 0);
        lv_obj_set_style_pad_top(cell, UIG_SUMMARY_PAD_T, 0);
        lv_obj_set_style_pad_right(cell, UIG_SUMMARY_PAD_R, 0);
        lv_obj_set_style_pad_bottom(cell, UIG_SUMMARY_PAD_B, 0);
        lv_obj_set_style_pad_left(cell, UIG_SUMMARY_PAD_L, 0);
        lv_obj_set_style_radius(cell, UIG_SUMMARY_RADIUS, 0);
        /* The border always takes its 1 px, only its colour shows the active room */
        lv_obj_set_style_border_width(cell, 1, 0);

        lv_obj_t *slot = lv_obj_create(cell);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, UIG_SUMMARY_FLAME_W, UIG_SUMMARY_FLAME_H);
        lv_obj_center(flame_create(slot, FLAME_MONO_9));

        lv_obj_t *letter = ui_tight_label_dy(cell, &font_pret600_12, TH_GRAY_OFF, 0, UIG_SUMMARY_TEXT_H,
                                             UIG_SUMMARY_TEXT_DY);
        ui_tight_label_set_text(letter, letters[i]);
    }
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -UIG_SUMMARY_BOTTOM);
    return row;
}

void summary_set(lv_obj_t *summary, const app_state_t *st)
{
    for (int i = 0; i < APP_ROOM_COUNT; i++) {
        const app_room_state_t *r = &st->rooms[i];
        lv_color_t c = r->on ? TH_GRAY_ON : TH_GRAY_OFF;
        lv_obj_t *cell = lv_obj_get_child(summary, i);
        lv_obj_t *flame = lv_obj_get_child(lv_obj_get_child(cell, 0), 0);
        lv_obj_t *letter = lv_obj_get_child(lv_obj_get_child(cell, 1), 0);

        lv_obj_set_style_border_color(cell, TH_GRAY_ON, 0);
        lv_obj_set_style_border_opa(cell, i == (int)st->active_room ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_hidden(flame, !(r->on && r->burning));
        flame_set_color(flame, c);
        lv_obj_set_style_text_color(letter, c, 0);
    }
}
