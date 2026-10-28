# Sources shared by the ESP-IDF build (CMakeLists.txt) and the PC sim
# (tools/sim/CMakeLists.txt). ui_router.c is device-only and not listed.
# file(GLOB) is evaluated at configure time: after adding a font or icon,
# re-run cmake configure.
set(UI_SHARED_SRCS
    screen_main.c
    ui_input.c
    ui_screens.c
    ui_test.c
    ui_util.c
    widget_boiler_badge.c
    widget_flame.c
    widget_flash.c
    widget_gauge.c
    widget_room_summary.c
    widget_wifi.c
)
file(GLOB UI_FONT_SRCS "${CMAKE_CURRENT_LIST_DIR}/fonts/font_*.c")
file(GLOB UI_ICON_SRCS "${CMAKE_CURRENT_LIST_DIR}/icons/img_*.c")
