cmake_minimum_required(VERSION 3.16)

# ESP-IDF CMakeLists for Rule Engine
# Set IDF_PATH and source ESP-IDF before building:
#   . $IDF_PATH/export.sh
#   idf.py build

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(rule_engine_esp32)

# Include paths
idf_build_set_property(INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/../../src/core"
    "${CMAKE_SOURCE_DIR}/../../src/hal/esp32"
    "${CMAKE_SOURCE_DIR}/../../src/scheduler"
    "${CMAKE_SOURCE_DIR}/../../src/logger"
    "${CMAKE_SOURCE_DIR}/../../generated"
    APPEND
)

# Source files
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/../../src"
)
