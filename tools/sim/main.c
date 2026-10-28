/* PC simulator: the shared UI on SDL2, driven by test commands on TCP 9999.
   Everything runs in this one thread, like the device's LVGL task. */
#include <SDL2/SDL.h>
#include <signal.h>
#include "lvgl.h"
#include "app_state.h"
#include "ui_priv.h"
#include "sim_remote.h"

static volatile sig_atomic_t s_quit;
static void on_signal(int sig) { (void)sig; s_quit = 1; }

/* Blacks out everything beyond the round panel's r = 120 so the window looks
   like the device. A thick circular border on the system layer: that layer
   sits above every screen but is not part of lv_screen_active(), so SNAP and
   the golden diff never see it. */
static void add_round_mask(void)
{
    const int bw = 60;                   /* reaches past the window corners (r ≈ 170) */
    lv_obj_t *m = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, 240 + 2 * bw, 240 + 2 * bw);
    lv_obj_center(m);
    lv_obj_set_style_radius(m, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(m, bw, 0);
    lv_obj_set_style_border_color(m, lv_color_black(), 0);
    lv_obj_set_style_border_opa(m, LV_OPA_COVER, 0);
    lv_obj_set_clickable(m, false);
}

int main(void)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    lv_init();
    lv_display_t *disp = lv_sdl_window_create(240, 240);
    if (!disp) return 1;
    lv_sdl_window_set_zoom(disp, 2);     /* view only; SNAP is always 240×240 */
    add_round_mask();

    app_state_init();
    ui_screens_create();
    app_state_subscribe(ui_screens_refresh);
    ui_screens_refresh();
    sim_remote_init(9999);

    uint32_t prev = SDL_GetTicks();
    while (!s_quit) {
        sim_remote_poll();
        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - prev);
        prev = now;
        lv_timer_handler();
        SDL_Delay(5);
    }
    sim_remote_shutdown();
    return 0;
}
