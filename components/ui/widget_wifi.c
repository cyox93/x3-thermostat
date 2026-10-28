#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

lv_obj_t *wifi_create(lv_obj_t *parent)
{
    lv_obj_t *row = ui_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UIG_WIFI_TEXT_GAP, 0);
    lv_image_create(row);
    ui_label(row, &font_jb400_9, TH_DIM, UIG_WIFI_LS);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -UIG_WIFI_BOTTOM);
    return row;
}

void wifi_set_level(lv_obj_t *wifi, uint8_t level)
{
    static const lv_image_dsc_t *const icons[] = { &img_wifi_0, &img_wifi_1, &img_wifi_2, &img_wifi_3 };
    if (level > 3) level = 3;
    lv_image_set_src(lv_obj_get_child(wifi, 0), icons[level]);
    if (level) lv_label_set_text_fmt(lv_obj_get_child(wifi, 1), "%d/3", level);
    else lv_label_set_text(lv_obj_get_child(wifi, 1), "OFF");
}
