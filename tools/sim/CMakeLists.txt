cmake_minimum_required(VERSION 3.20)
project(x3-thermostat-sim C CXX)
set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

set(X3_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
set(LVGL_DIR ${X3_ROOT}/managed_components/lvgl__lvgl)
if(NOT EXISTS ${LVGL_DIR}/lvgl.h)
    message(FATAL_ERROR "LVGL not found: build the firmware once so ${LVGL_DIR} is fetched")
endif()

set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_LIST_DIR}/lvgl_conf" CACHE PATH "sim lv_conf.h" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
add_subdirectory(${LVGL_DIR} lvgl_build)
target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS})

set(UI_DIR ${X3_ROOT}/components/ui)
include(${UI_DIR}/sources.cmake)
list(TRANSFORM UI_SHARED_SRCS PREPEND "${UI_DIR}/")

add_executable(sim
    main.c
    sim_remote.c
    ${UI_SHARED_SRCS}
    ${UI_FONT_SRCS}
    ${UI_ICON_SRCS}
    ${X3_ROOT}/components/state/app_state.c
    ${X3_ROOT}/components/state/app_input.c
)
target_include_directories(sim PRIVATE . lvgl_conf stubs ${SDL2_INCLUDE_DIRS}
    ${X3_ROOT}/components/include ${UI_DIR} ${LVGL_DIR})
target_compile_definitions(sim PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
target_link_libraries(sim PRIVATE lvgl SDL2::SDL2 m)

enable_testing()
add_subdirectory(tests)
