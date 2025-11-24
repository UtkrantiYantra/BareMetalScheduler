cmake_minimum_required(VERSION 3.13)
project(baremetal_rtos LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add option to choose target port: stm32f4 / esp32 / avr / host
if(NOT DEFINED TARGET_PORT)
  set(TARGET_PORT "host" CACHE STRING "Target port: stm32f4, esp32, avr, host")
endif()

message(STATUS "Building for TARGET_PORT=${TARGET_PORT}")

include_directories(include)
file(GLOB SRC src/*.c)

add_executable(rtos ${SRC})

# Platform specific compile flags (user may customize)
if(TARGET_PORT STREQUAL "stm32f4")
  target_compile_definitions(rtos PRIVATE TARGET_STM32F4)
  # user should supply MCU startup files and linker script
elseif(TARGET_PORT STREQUAL "esp32")
  target_compile_definitions(rtos PRIVATE TARGET_ESP32)
elseif(TARGET_PORT STREQUAL "avr")
  target_compile_definitions(rtos PRIVATE TARGET_AVR)
else()
  target_compile_definitions(rtos PRIVATE TARGET_HOST)
endif()
