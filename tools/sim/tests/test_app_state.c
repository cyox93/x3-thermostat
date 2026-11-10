#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_state.h"

static int s_notified;
static void on_change(void) { s_notified++; }

static void test_defaults(void)
{
    app_state_init();
    const app_state_t *s = app_state_get();
    assert(s->mode == APP_MODE_MAIN);
    assert(s->installed_room == APP_ROOM_LIVING && s->active_room == APP_ROOM_LIVING);
    assert(app_state_is_master());
    assert(s->rooms[APP_ROOM_LIVING].on && s->rooms[APP_ROOM_LIVING].target_x10 == 220);
    assert(!s->rooms[APP_ROOM_BIG].on && s->rooms[APP_ROOM_BIG].target_x10 == 220);
    assert(s->rooms[APP_ROOM_BIG].current_x10 == APP_TEMP_NONE);
    assert(s->rooms[APP_ROOM_BIG].humidity == APP_HUM_NONE);
    assert(s->brightness == 70 && s->screen_time_s == 30 && s->wifi_level == 3);
}

static void test_names(void)
{
    assert(strcmp(app_room_name(APP_ROOM_SMALL), "작은방") == 0);
    assert(app_room_from_name("일하는방") == APP_ROOM_WORK);
    assert(app_room_from_name("놀이방") == -1);
    assert(strcmp(app_mode_name(APP_MODE_EDIT_SCREENTIME), "edit-screenTime") == 0);
    assert(app_mode_from_name("edit-room") == APP_MODE_EDIT_ROOM);
    assert(app_mode_from_name("nope") == -1);
}

static void test_setters_notify_and_clamp(void)
{
    app_state_init();
    s_notified = 0;
    app_state_subscribe(on_change);

    app_state_set_installed(APP_ROOM_BIG);
    assert(!app_state_is_master());
    app_room_state_t rs = { .on = true, .target_x10 = 999, .current_x10 = 198,
                            .humidity = 52, .burning = true };
    app_state_set_room(APP_ROOM_BIG, &rs);
    assert(app_state_get()->rooms[APP_ROOM_BIG].target_x10 == APP_TARGET_MAX_X10);
    app_state_set_wifi(9);
    assert(app_state_get()->wifi_level == 3);
    app_state_set_brightness(3);
    assert(app_state_get()->brightness == 10);
    app_state_set_screen_time(45);              /* not a step → ignored */
    assert(app_state_get()->screen_time_s == 30);
    app_state_set_installed(APP_ROOM_COUNT);    /* invalid → ignored */
    assert(app_state_get()->installed_room == APP_ROOM_BIG);
    assert(s_notified == 4);                    /* the two ignored calls don't notify */

    app_state_subscribe(NULL);
}

int main(void)
{
    test_defaults();
    test_names();
    test_setters_notify_and_clamp();
    printf("test_app_state: ok\n");
    return 0;
}
