#include "ui_priv.h"

static lv_obj_t *s_screens[APP_MODE_COUNT];
static bool s_anim = true;

void ui_screens_create(void)
{
    s_screens[APP_MODE_MAIN] = screen_main_create();
    flash_init();
    /* Stage 2 adds SETTINGS / EDIT_* screens; until then they fall back to MAIN */
}

void ui_screens_refresh(void)
{
    lv_obj_t *want = s_screens[app_state_get()->mode];
    if (!want) want = s_screens[APP_MODE_MAIN];
    if (lv_screen_active() != want) lv_screen_load(want);
    screen_main_refresh();
}

void ui_screens_set_anim(bool on) { s_anim = on; }
bool ui_screens_anim(void) { return s_anim; }
