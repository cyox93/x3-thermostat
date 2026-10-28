#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* qa-flame: (1,1) → (0.92,1.08) → (1.05,0.94) → (1,1), pivot bottom centre */
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

/* "[flame] 보일러" at the top of the master's MAIN screen: lit while any room burns */
lv_obj_t *badge_create(lv_obj_t *parent)
{
    lv_obj_t *row = ui_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UIG_BADGE_GAP, 0);
    lv_obj_t *lit = flame_create(row, FLAME_COLOR_10);  /* child 0: burning, flickers */
    lv_image_set_pivot(lit, img_flame_color_10.header.w / 2, img_flame_color_10.header.h);
    lv_obj_t *idle = flame_create(row, FLAME_MONO_10);  /* child 1: idle, still */
    flame_set_color(idle, TH_GRAY_OFF);
    lv_obj_t *label = ui_label(row, &font_pret500_12, TH_BADGE_OFF, 0);
    lv_label_set_text(label, "보일러");
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, UIG_BADGE_TOP);
    return row;
}

void badge_set(lv_obj_t *badge, bool burning)
{
    lv_obj_t *lit = lv_obj_get_child(badge, 0);
    /* Restart only on a change, or the flicker would jump on every refresh */
    bool want = burning && ui_screens_anim();
    if (want != (lv_anim_get(lit, flicker_cb) != NULL)) flicker(lit, want);
    lv_obj_set_hidden(lit, !burning);
    lv_obj_set_hidden(lv_obj_get_child(badge, 1), burning);
    lv_obj_set_style_text_color(lv_obj_get_child(badge, 2), burning ? TH_BADGE_ON : TH_BADGE_OFF, 0);
}
