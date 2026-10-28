#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

static lv_obj_t *s_overlay, *s_disc, *s_label;

/* 0 → 1 in the first quarter, back to 0 by the end (design qa-press keyframes) */
static void fade_cb(void *obj, int32_t t)
{
    int opa = t < UIG_FLASH_IN_MS ? t * 255 / UIG_FLASH_IN_MS
            : (UIG_FLASH_MS - t) * 255 / (UIG_FLASH_MS - UIG_FLASH_IN_MS);
    lv_obj_set_style_opa(obj, opa, 0);
}

static void done_cb(lv_anim_t *a)
{
    lv_obj_set_hidden(a->var, true);
}

void flash_init(void)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, UIG_W, UIG_H);
    lv_obj_set_clickable(s_overlay, false);

    /* The design's radial gradient (25 % → 0 at 65 %) as one faint disc —
       the README allows a solid circle to spare the draw time */
    s_disc = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_disc);
    lv_obj_set_size(s_disc, 2 * UIG_FLASH_R, 2 * UIG_FLASH_R);
    lv_obj_center(s_disc);
    lv_obj_set_style_radius(s_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_disc, TH_OPA_PCT(UIG_FLASH_DISC_OPA_PCT), 0);

    s_label = ui_label(s_overlay, &font_pret600_32, TH_ACCENT, 0);
    lv_obj_center(s_label);
    lv_obj_set_hidden(s_overlay, true);
}

void flash_show(const char *text, lv_color_t accent)
{
    lv_label_set_text(s_label, text);
    lv_obj_set_style_text_color(s_label, accent, 0);
    lv_obj_set_style_bg_color(s_disc, accent, 0);
    lv_obj_set_style_opa(s_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_hidden(s_overlay, false);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_overlay);
    lv_anim_set_exec_cb(&a, fade_cb);
    lv_anim_set_values(&a, 0, UIG_FLASH_MS);
    lv_anim_set_duration(&a, UIG_FLASH_MS);
    lv_anim_set_completed_cb(&a, done_cb);
    lv_anim_delete(s_overlay, fade_cb);      /* a new press restarts the flash */
    lv_anim_start(&a);
}
