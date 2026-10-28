#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* "[flame] 보일러" at the top of the master's MAIN screen: lit while any room burns */
lv_obj_t *badge_create(lv_obj_t *parent)
{
    lv_obj_t *row = ui_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UIG_BADGE_GAP, 0);
    flame_create(row, FLAME_COLOR_10);                  /* child 0: burning */
    lv_obj_t *idle = flame_create(row, FLAME_MONO_10);  /* child 1: idle, still */
    flame_set_color(idle, TH_GRAY_OFF);
    lv_obj_t *label = ui_label(row, &font_pret500_12, TH_BADGE_OFF, 0);
    lv_label_set_text(label, "보일러");
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, UIG_BADGE_TOP);
    return row;
}

void badge_set(lv_obj_t *badge, bool burning)
{
    lv_obj_set_hidden(lv_obj_get_child(badge, 0), !burning);
    lv_obj_set_hidden(lv_obj_get_child(badge, 1), burning);
    lv_obj_set_style_text_color(lv_obj_get_child(badge, 2), burning ? TH_BADGE_ON : TH_BADGE_OFF, 0);
}
