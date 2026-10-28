#include <math.h>
#include <stdlib.h>
#include "ui_priv.h"
#include "ui_geometry.h"
#include "theme.h"

#define GAUGE_MAX_TICKS 16

struct gauge {
    gauge_cfg_t cfg;
    lv_obj_t *value;
    lv_obj_t *ticks[GAUGE_MAX_TICKS];
    lv_point_precise_t tick_pts[GAUGE_MAX_TICKS][2];
    lv_obj_t *marker;
    lv_point_precise_t marker_pts[2];
};

static void polar(int r, float deg, lv_point_precise_t *p)
{
    float rad = (deg - 90.0f) * (float)M_PI / 180.0f;
    p->x = UIG_CX + r * cosf(rad);
    p->y = UIG_CY + r * sinf(rad);
}

static float t_to_deg(int t_pm)
{
    return UIG_ARC_START_DEG + UIG_ARC_SWEEP_DEG * t_pm / 1000.0f;
}

/* lv_arc draws its stroke inside the object box, so the box diameter is
   2r + w for a stroke centred on r. */
static lv_obj_t *arc_new(lv_obj_t *parent, int w)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_set_size(a, 2 * UIG_ARC_R + w, 2 * UIG_ARC_R + w);
    lv_obj_center(a);
    lv_obj_set_style_pad_all(a, 0, 0);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(a, false);
    /* LVGL 0° is 3 o'clock, design 0° is 12 o'clock */
    lv_arc_set_rotation(a, (UIG_ARC_START_DEG + 270 + 360) % 360);
    lv_arc_set_bg_angles(a, 0, UIG_ARC_SWEEP_DEG);
    lv_arc_set_range(a, 0, 1000);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    return a;
}

static lv_obj_t *line_new(lv_obj_t *parent, lv_point_precise_t *pts, int w)
{
    lv_obj_t *l = lv_line_create(parent);
    lv_line_set_points(l, pts, 2);
    lv_obj_set_pos(l, 0, 0);
    lv_obj_set_style_line_width(l, w, 0);
    return l;
}

gauge_t *gauge_create(lv_obj_t *parent, const gauge_cfg_t *cfg)
{
    gauge_t *g = calloc(1, sizeof(*g));
    g->cfg = *cfg;

    lv_obj_t *track = arc_new(parent, UIG_ARC_TRACK_W);
    lv_obj_set_style_arc_color(track, TH_BG2, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(track, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(track, LV_OPA_TRANSP, LV_PART_INDICATOR);

    g->value = arc_new(parent, UIG_ARC_VALUE_W);
    lv_obj_set_style_arc_opa(g->value, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g->value, TH_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g->value, true, LV_PART_INDICATOR);

    for (int i = 0; i < cfg->tick_count && i < GAUGE_MAX_TICKS; i++) {
        float deg = t_to_deg(cfg->tick_count > 1 ? i * 1000 / (cfg->tick_count - 1) : 0);
        bool major = cfg->tick_major_every == 0 || i % cfg->tick_major_every == 0;
        polar(cfg->tick_r_in, deg, &g->tick_pts[i][0]);
        polar(major ? cfg->tick_r_out_major : cfg->tick_r_out_minor, deg, &g->tick_pts[i][1]);
        g->ticks[i] = line_new(parent, g->tick_pts[i], cfg->tick_w ? cfg->tick_w : UIG_TICK_W);
    }

    if (cfg->marker) {
        g->marker = line_new(parent, g->marker_pts, UIG_CUR_W);
        lv_obj_set_style_line_color(g->marker, TH_CURRENT, 0);
        lv_obj_set_style_line_opa(g->marker, TH_OPA_PCT(95), 0);
        lv_obj_set_style_line_rounded(g->marker, true, 0);
    }
    return g;
}

void gauge_set_value(gauge_t *g, bool show, int t_pm)
{
    lv_arc_set_value(g->value, t_pm);
    ui_show(g->value, show);

    int n = g->cfg.tick_count;
    for (int i = 0; i < n; i++) {
        /* design: active when i/(n-1) <= t */
        bool on = show && i * 1000 <= t_pm * (n - 1);
        bool major = g->cfg.tick_major_every == 0 || i % g->cfg.tick_major_every == 0;
        int opa = on ? g->cfg.tick_on_opa_pct : g->cfg.tick_off_opa_pct;
        if (!major) opa = opa * UIG_TICK_MINOR_OPA_PCT / 100;    /* minor ticks are thinner (0.6) */
        lv_obj_set_style_line_color(g->ticks[i], on ? g->cfg.tick_on_color : TH_DIM, 0);
        lv_obj_set_style_line_opa(g->ticks[i], TH_OPA_PCT(opa), 0);
    }
}

void gauge_set_marker(gauge_t *g, bool show, int t_pm)
{
    if (!g->marker) return;
    ui_show(g->marker, show);
    if (!show) return;
    if (t_pm < 0) t_pm = 0;
    if (t_pm > 1000) t_pm = 1000;
    float deg = t_to_deg(t_pm);
    polar(UIG_CUR_R_IN, deg, &g->marker_pts[0]);
    polar(UIG_CUR_R_OUT, deg, &g->marker_pts[1]);
    lv_line_set_points(g->marker, g->marker_pts, 2);
}
