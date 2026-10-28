/* PC simulator: the shared UI on SDL2, driven by the keyboard / wheel and by
   test commands on TCP 9999. Everything runs in this one thread, like the
   device's LVGL task.
     wheel, ←/→   encoder (one notch = one detent)
     Space        K1: release before 1 s = short, hold 1 s = long (fires while held)
     Enter        K1 short
     R            K2 = ROOM (master only)
     Esc          quit */
#include <SDL2/SDL.h>
#include <signal.h>
#include "lvgl.h"
#include "app_state.h"
#include "ui_priv.h"
#include "sim_remote.h"

#define LONG_PRESS_MS 1000      /* switchloop's long-press time (docs/ui.md §2.1) */

static volatile sig_atomic_t s_quit;
static uint32_t s_space_down;       /* 0 = not held */
static bool s_long_fired;

/* Takes only keys and the wheel off the SDL queue; LVGL's SDL driver drains
   the rest (window, mouse) inside lv_timer_handler */
static void poll_input(void)
{
    SDL_Event ev[16];
    SDL_PumpEvents();
    int n = SDL_PeepEvents(ev, 16, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP);
    for (int i = 0; i < n; i++) {
        SDL_Keycode k = ev[i].key.keysym.sym;
        bool down = ev[i].type == SDL_KEYDOWN;
        if (down && ev[i].key.repeat) continue;
        if (k == SDLK_SPACE) {
            if (down) {
                s_space_down = SDL_GetTicks() | 1;
                s_long_fired = false;
            } else if (s_space_down) {
                if (!s_long_fired) ui_input_apply(UI_IN_SHORT);
                s_space_down = 0;
            }
        } else if (!down) {
            continue;
        } else if (k == SDLK_RETURN) ui_input_apply(UI_IN_SHORT);
        else if (k == SDLK_RIGHT) ui_input_apply(UI_IN_ENC_CW);
        else if (k == SDLK_LEFT) ui_input_apply(UI_IN_ENC_CCW);
        else if (k == SDLK_r) ui_input_apply(UI_IN_ROOM);
        else if (k == SDLK_ESCAPE) s_quit = 1;
    }
    n = SDL_PeepEvents(ev, 16, SDL_GETEVENT, SDL_MOUSEWHEEL, SDL_MOUSEWHEEL);
    for (int i = 0; i < n; i++) {
        int y = ev[i].wheel.y;
        for (; y > 0; y--) ui_input_apply(UI_IN_ENC_CW);
        for (; y < 0; y++) ui_input_apply(UI_IN_ENC_CCW);
    }
    if (s_space_down && !s_long_fired && SDL_GetTicks() - s_space_down >= LONG_PRESS_MS) {
        s_long_fired = true;
        ui_input_apply(UI_IN_LONG);
    }
}

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
        poll_input();
        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - prev);
        prev = now;
        lv_timer_handler();
        SDL_Delay(5);
    }
    sim_remote_shutdown();
    return 0;
}
