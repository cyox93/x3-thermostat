#include "ui_priv.h"
#include "theme.h"

lv_obj_t *flame_create(lv_obj_t *parent, flame_kind_t kind)
{
    static const lv_image_dsc_t *const src[] = {
        [FLAME_COLOR_14] = &img_flame_color_14, [FLAME_COLOR_10] = &img_flame_color_10,
        [FLAME_MONO_14] = &img_flame_mono_14, [FLAME_MONO_10] = &img_flame_mono_10, [FLAME_MONO_9] = &img_flame_mono_9,
    };
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src[kind]);
    if (kind == FLAME_MONO_14 || kind == FLAME_MONO_10 || kind == FLAME_MONO_9) {
        /* A8 images take their colour from image_recolor */
        lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    }
    return img;
}

void flame_set_color(lv_obj_t *flame, lv_color_t c)
{
    lv_obj_set_style_image_recolor(flame, c, 0);
}
