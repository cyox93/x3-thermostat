#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_state.h"

#define ST (app_state_get())
#define ROOM(r) (ST->rooms[r])

static void flash_is(app_result_t r, const char *text, app_accent_t accent)
{
    assert(r.flash && strcmp(r.flash, text) == 0);
    assert(r.accent == accent);
}

static void test_main_encoder(void)
{
    app_state_init();                               /* 거실 on 22.0 */
    app_result_t r = app_input_enc(+1);
    assert(ROOM(APP_ROOM_LIVING).target_x10 == 225);
    assert(r.heat_changed && r.heat_room == APP_ROOM_LIVING && !r.flash);
    for (int i = 0; i < 40; i++) app_input_enc(+1);
    assert(ROOM(APP_ROOM_LIVING).target_x10 == 300);
    for (int i = 0; i < 40; i++) app_input_enc(-1);
    assert(ROOM(APP_ROOM_LIVING).target_x10 == 150);
    r = app_input_enc(-1);                          /* at the limit: nothing changes */
    assert(!r.heat_changed);

    app_input_short();                              /* off: the knob does nothing */
    r = app_input_enc(+1);
    assert(ROOM(APP_ROOM_LIVING).target_x10 == 150 && !r.heat_changed);
}

static void test_main_short_long(void)
{
    app_state_init();
    app_result_t r = app_input_short();
    assert(!ROOM(APP_ROOM_LIVING).on && r.heat_changed);
    flash_is(r, "OFF", APP_ACCENT_BOILER);
    r = app_input_short();
    assert(ROOM(APP_ROOM_LIVING).on);
    flash_is(r, "ON", APP_ACCENT_BOILER);

    r = app_input_long();
    assert(ST->mode == APP_MODE_SETTINGS && ST->settings_sel == 0);
    flash_is(r, "설정", APP_ACCENT_SETTING);
    r = app_input_long();                           /* long only acts in MAIN */
    assert(ST->mode == APP_MODE_SETTINGS && !r.flash);
}

static void test_room_button(void)
{
    app_state_init();                               /* master */
    app_result_t r = app_input_room();
    assert(ST->active_room == APP_ROOM_BIG);
    flash_is(r, "큰방", APP_ACCENT_SETTING);
    app_input_short();                              /* toggles the active room, not the installed one */
    assert(ROOM(APP_ROOM_BIG).on && ROOM(APP_ROOM_LIVING).on);
    for (int i = 0; i < 3; i++) app_input_room();
    assert(ST->active_room == APP_ROOM_LIVING);     /* wraps */

    app_input_long();                               /* not in MAIN: ignored */
    r = app_input_room();
    assert(ST->active_room == APP_ROOM_LIVING && !r.flash);

    app_state_init();
    app_state_set_installed(APP_ROOM_WORK);
    app_state_set_active(APP_ROOM_WORK);
    r = app_input_room();                           /* not master: ignored */
    assert(ST->active_room == APP_ROOM_WORK && !r.flash);
}

static void test_settings_list(void)
{
    app_state_init();
    app_input_long();
    app_input_enc(-1);
    assert(ST->settings_sel == 3);                  /* wraps backwards */
    app_input_enc(+1);
    assert(ST->settings_sel == 0);

    app_result_t r = app_input_short();             /* 방선택 */
    assert(ST->mode == APP_MODE_EDIT_ROOM && ST->room_edit_sel == ST->installed_room && !r.flash);
    app_input_enc(+1);
    app_input_enc(+1);
    assert(ST->room_edit_sel == APP_ROOM_SMALL);
    r = app_input_short();
    assert(ST->mode == APP_MODE_SETTINGS && ST->settings_sel == 0);
    assert(ST->installed_room == APP_ROOM_SMALL && ST->active_room == APP_ROOM_SMALL);
    assert(!app_state_is_master() && r.settings_commit);
    flash_is(r, "작은방", APP_ACCENT_SETTING);

    app_input_enc(+1);                              /* 화면밝기 */
    app_input_short();
    assert(ST->mode == APP_MODE_EDIT_BRIGHTNESS);
    for (int i = 0; i < 10; i++) app_input_enc(+1);
    assert(ST->brightness == 100);
    for (int i = 0; i < 30; i++) app_input_enc(-1);
    assert(ST->brightness == 10);
    r = app_input_short();
    assert(ST->mode == APP_MODE_SETTINGS && ST->settings_sel == 1 && r.settings_commit);
    flash_is(r, "확인", APP_ACCENT_SETTING);

    app_input_enc(+1);                              /* 화면시간 */
    app_input_short();
    assert(ST->mode == APP_MODE_EDIT_SCREENTIME);
    for (int i = 0; i < 9; i++) app_input_enc(+1);
    assert(ST->screen_time_s == APP_SCREEN_TIME_ALWAYS);     /* last step, after 300 */
    for (int i = 0; i < 9; i++) app_input_enc(-1);
    assert(ST->screen_time_s == 10);
    app_input_short();

    app_input_enc(+1);                              /* 이전 */
    r = app_input_short();
    assert(ST->mode == APP_MODE_MAIN);
    flash_is(r, "이전", APP_ACCENT_SETTING);
    assert(!r.settings_commit);
}

int main(void)
{
    test_main_encoder();
    test_main_short_long();
    test_room_button();
    test_settings_list();
    printf("test_app_input: ok\n");
    return 0;
}
