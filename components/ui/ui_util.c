#include <stdio.h>
#include <stdlib.h>
#include "ui_priv.h"
#include "ui_geometry.h"

lv_obj_t *ui_box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(b, flow);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, false);
    return b;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_letter_space(l, letter_space, 0);
    /* CSS letter-spacing also trails the last glyph; keep that space in the
       box or centred text sits half a space off from the design */
    if (letter_space > 0) lv_obj_set_style_pad_right(l, letter_space, 0);
    return l;
}

lv_obj_t *ui_tight_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                         int letter_space, int box_h)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_height(box, box_h);
    lv_obj_set_overflow_visible(box, true);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_clickable(box, false);
    lv_obj_t *l = ui_label(box, font, color, letter_space);
    lv_obj_set_style_pad_right(l, 0, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, UIG_TIGHT_DY);
    return l;
}

void ui_tight_label_set_text(lv_obj_t *label, const char *text)
{
    /* A centred child doesn't count toward LV_SIZE_CONTENT, so size the box
       from the text. Like CSS, the width includes the trailing letter space,
       which for negative spacing pulls the next item closer. */
    lv_label_set_text(label, text);
    lv_obj_t *box = lv_obj_get_parent(label);
    lv_point_t sz;
    lv_text_get_size(&sz, text, lv_obj_get_style_text_font(label, 0),
                     lv_obj_get_style_text_letter_space(label, 0), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(box, sz.x + lv_obj_get_style_text_letter_space(label, 0));
}

void ui_fmt_x10(char *buf, size_t len, int v_x10)
{
    int a = abs(v_x10);
    snprintf(buf, len, "%s%d.%d", v_x10 < 0 ? "-" : "", a / 10, a % 10);
}

void ui_show(lv_obj_t *obj, bool show)
{
    lv_obj_set_hidden(obj, !show);
}
