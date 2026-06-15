# =============================================================================
# Rule Engine + Scheduler + Modules Framework — Linux Simulation Makefile
# =============================================================================
# Targets:
#   make              Build the simulation executable
#   make run          Build and run full simulation
#   make generate     Run all code generators (EventList + XML config)
#   make test         Build and run unit tests
#   make clean        Remove build artifacts
#   make distclean    Remove build + generated files
# =============================================================================

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -Wshadow \
           -Wstrict-prototypes -Wmissing-prototypes \
           -Wconversion -Wno-unused-parameter \
           -O2 -g
LDFLAGS := -lm

# Platform flag — Linux simulation
CFLAGS  += -DLINUX_PLATFORM

# ── Directories ──────────────────────────────────────────────────────────────
SRC_CORE      := src/core
SRC_HAL_LIN   := src/hal/linux
SRC_OSAL      := src/osal
SRC_SCHED     := src/scheduler
SRC_EVENT     := src/event
SRC_MOD_RELAY := src/modules/relay
SRC_MOD_LED   := src/modules/led
SRC_MOD_DIG   := src/modules/digital_sensor
SRC_MOD_ANA   := src/modules/analog_sensor
SRC_MOD_ALM   := src/modules/alarm
SRC_MOD_QUE   := src/modules/queue
SRC_MOD_BUZ   := src/modules/buzzer
SRC_MOD_SM    := src/modules/statemachine
SRC_MOD_DIA   := src/modules/diag
SRC_MOD_TOU   := src/modules/tou
SRC_DRIVERS   := src/drivers
SRC_MOD_LOG   := src/modules/logging
SRC_LOGGER    := src/logger
GEN_DIR       := generated
BUILD_DIR     := build
EVENT_CFG_DIR := src/event/configs

BIN           := $(BUILD_DIR)/rule_engine_sim

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := \
  -I$(SRC_CORE)      \
  -I$(SRC_HAL_LIN)   \
  -I$(SRC_OSAL)      \
  -I$(SRC_SCHED)     \
  -I$(SRC_EVENT)     \
  -I$(SRC_MOD_RELAY) \
  -I$(SRC_MOD_LED)   \
  -I$(SRC_MOD_DIG)   \
  -I$(SRC_MOD_ANA)   \
  -I$(SRC_MOD_ALM)   \
  -I$(SRC_MOD_QUE)   \
  -I$(SRC_MOD_BUZ)   \
  -I$(SRC_MOD_SM)    \
  -I$(SRC_MOD_DIA)   \
  -I$(SRC_MOD_TOU)   \
  -I$(SRC_DRIVERS)   \
  -I$(SRC_MOD_LOG)   \
  -I$(SRC_LOGGER)    \
  -I$(GEN_DIR)       \
  -I$(EVENT_CFG_DIR)

# ── Source files ─────────────────────────────────────────────────────────────
SRCS := \
  $(SRC_CORE)/re_engine.c          \
  $(SRC_HAL_LIN)/hal_linux.c       \
  $(SRC_SCHED)/scheduler.c         \
  $(SRC_EVENT)/event_bus.c         \
  $(SRC_MOD_RELAY)/relay_mgr.c     \
  $(SRC_MOD_LED)/led_mgr.c         \
  $(SRC_MOD_DIG)/digital_sensor_mgr.c \
  $(SRC_MOD_ANA)/analog_sensor_mgr.c  \
  $(SRC_MOD_ALM)/alarm_event_mgr.c    \
  $(SRC_MOD_LOG)/logging_mgr.c        \
  $(SRC_MOD_QUE)/queue_mgr.c          \
  $(SRC_MOD_BUZ)/buzzer_mgr.c         \
  $(SRC_MOD_SM)/state_machine.c       \
  $(SRC_MOD_DIA)/diag_mgr.c           \
  $(SRC_MOD_TOU)/tou_engine.c         \
  $(SRC_DRIVERS)/sensor_driver.c      \
  $(GEN_DIR)/EventRegistry.c          \
  $(GEN_DIR)/config.c              \
  $(GEN_DIR)/rule_table.c          \
  $(SRC_CORE)/main.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# =============================================================================
# Default target
# =============================================================================
.PHONY: all run generate generate-events generate-rules test clean distclean help lint

all: generate $(BIN)
	@echo "[BUILD] Done. Binary: $(BIN)"

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# =============================================================================
# Code generation — two stages
# =============================================================================
generate: generate-events generate-rules

# Stage 1: merge EventConfig*.h → EventList.h
generate-events: $(GEN_DIR)/EventList.h

$(GEN_DIR)/EventList.h: $(wildcard $(EVENT_CFG_DIR)/EventConfig*.h) tools/fw_gen.py
	@echo "[GEN] Merging EventConfig*.h → EventList.h ..."
	python3 tools/fw_gen.py \
		--event-dir $(EVENT_CFG_DIR) \
		--out       $(GEN_DIR)

# Stage 2: XML rule config → config.h/c + rule_table.h/c
generate-rules: $(GEN_DIR)/rule_table.c

$(GEN_DIR)/rule_table.c: xml/examples/industrial_automation.xml tools/rule_engine_gen.py
	@echo "[GEN] Generating rule tables from XML ..."
	python3 tools/rule_engine_gen.py \
		--config xml/examples/industrial_automation.xml \
		--out $(GEN_DIR)

# =============================================================================
# Run
# =============================================================================
run: all
	@echo "[RUN] -----------------------------------------------"
	./$(BIN)

# =============================================================================
# Unit tests
# =============================================================================
TEST_SRCS := \
  tests/unit/test_conditions.c  \
  tests/unit/test_scheduler.c   \
  tests/unit/test_priority.c    \
  $(SRC_CORE)/re_engine.c       \
  $(SRC_HAL_LIN)/hal_linux.c    \
  $(SRC_SCHED)/scheduler.c      \
  $(SRC_EVENT)/event_bus.c      \
  $(SRC_MOD_LOG)/logging_mgr.c  \
  $(GEN_DIR)/config.c           \
  $(GEN_DIR)/rule_table.c

TEST_BIN  := $(BUILD_DIR)/test_runner
TEST_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))

test: generate $(TEST_BIN)
	@echo "[TEST] Running unit tests ..."
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# =============================================================================
# Static analysis
# =============================================================================
lint:
	cppcheck --enable=all --std=c99 \
	         --suppress=missingIncludeSystem \
	         $(INCLUDES) $(SRCS) 2>&1 | tee $(BUILD_DIR)/cppcheck.log
	@echo "[LINT] Report: $(BUILD_DIR)/cppcheck.log"

# =============================================================================
# Custom XML config
# =============================================================================
CONFIG ?= xml/examples/industrial_automation.xml
gen-custom: tools/rule_engine_gen.py
	python3 tools/rule_engine_gen.py --config $(CONFIG) --out $(GEN_DIR)

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Build artifacts removed."

distclean: clean
	rm -f $(GEN_DIR)/config.h $(GEN_DIR)/config.c
	rm -f $(GEN_DIR)/rule_table.h $(GEN_DIR)/rule_table.c
	rm -f $(GEN_DIR)/EventList.h
	@echo "[CLEAN] Generated files removed."

# =============================================================================
# Help
# =============================================================================
help:
	@echo "Framework Build Targets:"
	@echo "  make                  — Generate + build Linux simulation"
	@echo "  make run              — Build and run"
	@echo "  make generate         — Run all code generators"
	@echo "  make generate-events  — Merge EventConfig*.h → EventList.h only"
	@echo "  make generate-rules   — XML → rule tables only"
	@echo "  make test             — Build and run unit tests (28 tests)"
	@echo "  make lint             — cppcheck static analysis"
	@echo "  make clean            — Remove build artifacts"
	@echo "  make distclean        — Remove build + generated files"
	@echo "  make gen-custom CONFIG=xml/examples/my.xml"
