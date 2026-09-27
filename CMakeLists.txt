cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    components
    $ENV{IDF_PATH}/examples/system/console/advanced/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(x3-thermostat)
