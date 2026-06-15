cmake_minimum_required(VERSION 3.13)
project(rule_engine_framework LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Target port selection: host / stm32f4 / esp32 / avr ──────────────────────
if(NOT DEFINED TARGET_PORT)
  set(TARGET_PORT "host" CACHE STRING "Target port: stm32f4, esp32, avr, host")
endif()
message(STATUS "Building for TARGET_PORT=${TARGET_PORT}")

# ── RTOS backend selection: baremetal / freertos / threadx ───────────────────
if(NOT DEFINED RTOS_BACKEND)
  set(RTOS_BACKEND "baremetal" CACHE STRING "RTOS backend: baremetal, freertos, threadx")
endif()
message(STATUS "RTOS_BACKEND=${RTOS_BACKEND}")

# ── Code generation at configure time ─────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)

execute_process(
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fw_gen.py
          --event-dir ${CMAKE_SOURCE_DIR}/src/event/configs
          --out       ${CMAKE_SOURCE_DIR}/generated
  RESULT_VARIABLE FW_GEN_RESULT)
if(NOT FW_GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "fw_gen.py failed")
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/rule_engine_gen.py
          --config ${CMAKE_SOURCE_DIR}/xml/examples/industrial_automation.xml
          --out    ${CMAKE_SOURCE_DIR}/generated
  RESULT_VARIABLE RULE_GEN_RESULT)
if(NOT RULE_GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "rule_engine_gen.py failed")
endif()

# ── Include paths ─────────────────────────────────────────────────────────────
include_directories(
  src/core
  src/hal/linux
  src/osal
  src/scheduler
  src/event
  src/event/configs
  src/modules/relay
  src/modules/led
  src/modules/buzzer
  src/modules/digital_sensor
  src/modules/analog_sensor
  src/modules/alarm
  src/modules/logging
  src/modules/queue
  src/modules/statemachine
  src/modules/diag
  src/modules/tou
  src/drivers
  src/logger
  generated
  include
  src/ports
)

# ── Framework sources ─────────────────────────────────────────────────────────
set(FRAMEWORK_SRCS
  src/core/re_engine.c
  src/scheduler/scheduler.c
  src/event/event_bus.c
  src/modules/relay/relay_mgr.c
  src/modules/led/led_mgr.c
  src/modules/buzzer/buzzer_mgr.c
  src/modules/digital_sensor/digital_sensor_mgr.c
  src/modules/analog_sensor/analog_sensor_mgr.c
  src/modules/alarm/alarm_event_mgr.c
  src/modules/logging/logging_mgr.c
  src/modules/queue/queue_mgr.c
  src/modules/statemachine/state_machine.c
  src/modules/diag/diag_mgr.c
  src/modules/tou/tou_engine.c
  src/drivers/sensor_driver.c
  generated/config.c
  generated/rule_table.c
  generated/EventRegistry.c
)

# ── Platform-specific sources & defines ───────────────────────────────────────
if(TARGET_PORT STREQUAL "stm32f4")
  add_compile_definitions(TARGET_STM32F4 STM32_PLATFORM)
  list(APPEND FRAMEWORK_SRCS src/hal/stm32/hal_stm32.c)
  # user supplies MCU startup files, linker script, and arm-none-eabi toolchain file
elseif(TARGET_PORT STREQUAL "esp32")
  add_compile_definitions(TARGET_ESP32 ESP32_PLATFORM)
  list(APPEND FRAMEWORK_SRCS src/hal/esp32/hal_esp32.c)
elseif(TARGET_PORT STREQUAL "avr")
  add_compile_definitions(TARGET_AVR)
else()
  add_compile_definitions(TARGET_HOST LINUX_PLATFORM)
  list(APPEND FRAMEWORK_SRCS src/hal/linux/hal_linux.c)
endif()

# ── RTOS backend define ───────────────────────────────────────────────────────
if(RTOS_BACKEND STREQUAL "freertos")
  add_compile_definitions(OSAL_FREERTOS)
elseif(RTOS_BACKEND STREQUAL "threadx")
  add_compile_definitions(OSAL_THREADX)
endif()

# ── Main simulation executable ────────────────────────────────────────────────
add_executable(rule_engine_sim ${FRAMEWORK_SRCS} src/core/main.c)
target_link_libraries(rule_engine_sim m)
target_compile_options(rule_engine_sim PRIVATE -Wall -Wextra -O2 -g)

# ── Imported baremetal RTOS components (host-only standalone lib) ─────────────
if(TARGET_PORT STREQUAL "host")
  add_library(baremetal_rtos STATIC
    src/baremetal_rtos/task.c
    src/baremetal_rtos/timerwheel.c
    src/baremetal_rtos/mempool.c
    src/baremetal_rtos/payload_helper.c
    src/baremetal_rtos/event_registry.c
    src/baremetal_rtos/logging.c
    src/baremetal_rtos/scheduler_system.c
  )
  target_compile_definitions(baremetal_rtos PRIVATE TARGET_HOST)
endif()

# ── Unit tests ────────────────────────────────────────────────────────────────
enable_testing()
add_executable(test_runner
  ${FRAMEWORK_SRCS}
  tests/unit/test_conditions.c
  tests/unit/test_scheduler.c
  tests/unit/test_priority.c
)
target_link_libraries(test_runner m)
add_test(NAME unit_tests COMMAND test_runner)
