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

int main(void)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    lv_init();
    lv_display_t *disp = lv_sdl_window_create(240, 240);
    if (!disp) return 1;
    lv_sdl_window_set_zoom(disp, 2);     /* view only; SNAP is always 240×240 */

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
