#include <stdlib.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

/* Rows hang off a track that slides by item_h per selection step, like the
   design's translateY; each row scales and fades by its distance from the
   selection. The caller fills the rows and styles the active one. */
struct focus_list {
    lv_obj_t *track;
    lv_obj_t **rows;
    int count, item_h, sel;
};

focus_list_t *focus_list_create(lv_obj_t *parent, int count, int item_h)
{
    focus_list_t *l = calloc(1, sizeof(*l));
    l->rows = calloc(count, sizeof(lv_obj_t *));
    l->count = count;
    l->item_h = item_h;
    l->sel = -1;

    l->track = lv_obj_create(parent);
    lv_obj_remove_style_all(l->track);
    /* Tall enough to hold every row: rows outside the track's own area were
       culled even with overflow visible */
    lv_obj_set_size(l->track, UIG_W, UIG_H + item_h * count);
    lv_obj_set_scrollable(l->track, false);
    lv_obj_set_clickable(l->track, false);

    for (int i = 0; i < count; i++) {
        lv_obj_t *row = lv_obj_create(l->track);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, UIG_W, LV_SIZE_CONTENT);
        lv_obj_set_pos(row, 0, UIG_CY - item_h / 2 + item_h * i);
        lv_obj_set_scrollable(row, false);
        lv_obj_set_clickable(row, false);
        lv_obj_set_style_transform_pivot_x(row, lv_pct(50), 0);
        lv_obj_set_style_transform_pivot_y(row, lv_pct(50), 0);
        l->rows[i] = row;
    }
    return l;
}

lv_obj_t *focus_list_row(focus_list_t *l, int i)
{
    return l->rows[i];
}

static void track_y_cb(void *obj, int32_t v)
{
    lv_obj_set_y(obj, v);
}

void focus_list_select(focus_list_t *l, int sel, bool animate)
{
    int to = -l->item_h * sel;
    lv_anim_delete(l->track, track_y_cb);
    if (animate && l->sel >= 0 && l->sel != sel) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, l->track);
        lv_anim_set_exec_cb(&a, track_y_cb);
        lv_anim_set_values(&a, lv_obj_get_y(l->track), to);
        lv_anim_set_duration(&a, UIG_LIST_ANIM_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    } else {
        lv_obj_set_y(l->track, to);
    }
    l->sel = sel;

    for (int i = 0; i < l->count; i++) {
        int d = abs(i - sel);
        lv_obj_set_style_transform_scale(l->rows[i], d == 0 ? LV_SCALE_NONE : d == 1 ? UIG_LIST_SCALE_1 : UIG_LIST_SCALE_2, 0);
        lv_obj_set_style_opa(l->rows[i], d == 0 ? LV_OPA_COVER : TH_OPA_PCT(d == 1 ? UIG_LIST_OPA_1_PCT : UIG_LIST_OPA_2_PCT), 0);
    }
}

/* Forget the shown selection, so the next select jumps instead of sliding
   (used when the screen is entered) */
void focus_list_reset(focus_list_t *l)
{
    l->sel = -1;
}

lv_obj_t *header_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *h = ui_label(parent, &font_pret500_18, TH_DIM, UIG_HEADER_LS);
    lv_obj_set_style_opa(h, TH_OPA_PCT(UIG_HEADER_OPA_PCT), 0);
    lv_label_set_text(h, text);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, UIG_HEADER_TOP);
    return h;
}
