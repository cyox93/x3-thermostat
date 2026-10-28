# Host unit tests for the pure-C UI state. Builds standalone
# (cmake -S tools/sim/tests) or as part of the sim (add_subdirectory).
cmake_minimum_required(VERSION 3.20)
project(x3-ui-tests C)
set(CMAKE_C_STANDARD 11)
enable_testing()

set(X3_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_executable(test_app_state test_app_state.c ${X3_ROOT}/components/state/app_state.c)
target_include_directories(test_app_state PRIVATE ${X3_ROOT}/components/include)
add_test(NAME app_state COMMAND test_app_state)

add_executable(test_app_input test_app_input.c
    ${X3_ROOT}/components/state/app_state.c ${X3_ROOT}/components/state/app_input.c)
target_include_directories(test_app_input PRIVATE ${X3_ROOT}/components/include)
add_test(NAME app_input COMMAND test_app_input)
