include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
list(TRANSFORM UI_SHARED_SRCS PREPEND "${CMAKE_CURRENT_LIST_DIR}/")

idf_component_register(SRCS ${UI_SHARED_SRCS} ui_router.c ${UI_FONT_SRCS} ${UI_ICON_SRCS}
                       INCLUDE_DIRS "../include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES state device lvgl esp_lvgl_port
                                espressif__knob espressif__button log freertos)

# lv_font_conv output picks "lvgl.h" over "lvgl/lvgl.h" with this symbol.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
