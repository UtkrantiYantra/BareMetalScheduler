# Rule Engine Framework

**Production-grade, configuration-driven firmware automation framework**  
Author: Utkranti Yantra Pvt Ltd  
Version: 1.0.0  
License: MIT

---

## Overview

A deterministic, zero-dynamic-allocation rule engine for embedded firmware.
Supports STM32, ESP32, and Linux simulation. Configuration is defined once in
XML and converted to C source via a Python code generator — firmware logic
never changes between deployments.

Suitable for:
- Industrial automation (pump, fan, contactor control)
- Fire alarm systems (multi-zone, priority-based)
- Smart metering / TOU scheduling
- Environmental monitoring
- Building automation

---

## Quick Start (Linux Simulation)

```bash
# 1. Generate C config from XML
python3 tools/rule_engine_gen.py \
    --config xml/examples/industrial_automation.xml \
    --out generated/

# 2. Build and run
make run
```

---

## Project Structure

```
rule_engine/
├── src/
│   ├── core/
│   │   ├── re_types.h          # All fundamental types
│   │   ├── re_hal.h            # HAL interface (platform-agnostic)
│   │   ├── re_engine.h         # Rule engine API
│   │   ├── re_engine.c         # Rule engine implementation
│   │   └── main.c              # Linux simulation entry point
│   ├── hal/
│   │   ├── linux/hal_linux.c   # Linux simulation HAL
│   │   ├── stm32/hal_stm32.c   # STM32 HAL (HAL/LL drivers)
│   │   └── esp32/hal_esp32.c   # ESP32 HAL (ESP-IDF)
│   ├── scheduler/
│   │   └── re_scheduler.h      # TOU scheduler interface
│   └── logger/
│       └── re_logger.h         # Platform logging macros
├── tools/
│   └── rule_engine_gen.py      # XML → C code generator
├── xml/
│   ├── schema/
│   │   └── rule_engine_config.xsd  # XSD validation schema
│   └── examples/
│       └── industrial_automation.xml
├── generated/                  # Auto-generated (do not edit)
│   ├── config.h
│   ├── config.c
│   ├── rule_table.h
│   └── rule_table.c
├── platform/
│   ├── stm32/Src/main_stm32.c
│   └── esp32/main/main_esp32.c
├── tests/
│   └── unit/test_conditions.c
├── docs/
│   └── README.md
└── Makefile
```

---

## Configuration XML

All system behaviour is defined in a single XML file.
Modify XML, re-run the generator, rebuild — **no firmware logic changes**.

### Sensor definition

```xml
<Sensor id="1"  name="BoilerTemp"  type="TEMPERATURE"
        unit="°C"   min="-10"  max="200"
        threshold_hi="50"   threshold_lo="10"
        hysteresis="5"      sample_interval_ms="2000"
        adc_channel="0"     scale_factor="20000"
        interface="ADC"     enabled="true"/>
```

### Sensor ID ranges

| Range | Type |
|-------|------|
| 1–19  | Temperature |
| 20–39 | Humidity |
| 40–49 | Current |
| 50–59 | Voltage |
| 60–69 | Fire |
| 70–79 | Smoke |
| 80–89 | Ultrasonic / Distance |
| 90–99 | Digital / PIR / Door |

### Output definition

```xml
<Output id="1" name="MainPump"  type="PUMP"
        priority="20"  default_state="OFF"  failsafe_state="OFF"
        gpio_pin="20"  active_high="true"/>
```

### Rule definition

```xml
<!-- Single sensor → single output -->
<Rule id="1" name="BoilerTempHighAlarm" priority="80" logic="AND">
    <Conditions>
        <Condition sensor_id="1" op="GT" value="50"/>
    </Conditions>
    <Actions>
        <Action output_id="2" state="ON"/>
    </Actions>
</Rule>

<!-- Multi-sensor AND logic -->
<Rule id="3" name="MotorOverheat" priority="95" logic="AND"
      on_delay_ms="5000">
    <Conditions>
        <Condition sensor_id="3" op="GT" value="70"/>
        <Condition sensor_id="40" op="GT" value="80"/>
    </Conditions>
    <Actions>
        <Action output_id="3" state="ON"/>
        <Action output_id="4" state="ON"/>
        <Action output_id="10" state="OFF"/>
    </Actions>
</Rule>

<!-- PULSE action -->
<Rule id="2" name="CriticalBuzzer" priority="90">
    <Conditions>
        <Condition sensor_id="1" op="GT" value="70"/>
    </Conditions>
    <Actions>
        <Action output_id="5" state="PULSE" duration_ms="5000"/>
    </Actions>
</Rule>

<!-- TOU-gated: only active during schedule window -->
<Rule id="7" name="WaterTankLow_StartPump" priority="20"
      schedule_id="3" on_delay_ms="3000">
    <Conditions>
        <Condition sensor_id="80" op="LT" value="20"/>
    </Conditions>
    <Actions>
        <Action output_id="1" state="ON"/>
    </Actions>
</Rule>
```

### TOU Schedule definition

```xml
<!-- Business hours: Mon–Fri 08:00–18:00 -->
<Schedule id="1" name="BusinessHours" type="WEEKLY" enabled="true">
    <Slot start="08:00" end="18:00" weekday_mask="0111110"/>
</Schedule>

<!-- Night-only: 18:00–08:00 all days (overnight span) -->
<Schedule id="2" name="NightHours" type="DAILY">
    <Slot start="18:00" end="08:00"/>
</Schedule>
```

---

## Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `EQ` | Equal | `op="EQ" value="100"` |
| `NE` | Not equal | `op="NE" value="0"` |
| `GT` | Greater than | `op="GT" value="50"` |
| `GTE` | Greater than or equal | |
| `LT` | Less than | `op="LT" value="20"` |
| `LTE` | Less than or equal | |
| `BETWEEN` | Range inclusive | `op="BETWEEN" value_lo="18" value_hi="28"` |
| `NOT_BETWEEN` | Outside range | |
| `ON` | Digital input HIGH | `op="ON"` |
| `OFF` | Digital input LOW | `op="OFF"` |

**Logic operators** (applied across conditions in a rule):  
`AND` `OR` `XOR` `NOT`

---

## Output Actions

| Action | Description |
|--------|-------------|
| `ON` | Turn output ON |
| `OFF` | Turn output OFF |
| `TOGGLE` | Toggle current state |
| `PULSE` | ON for `duration_ms`, then auto-OFF |

---

## Priority Arbitration

When multiple rules target the same output, the highest-priority rule wins.
A lower-priority rule cannot override a higher-priority active output.

```
Fire Alarm priority = 100  → always wins
General Alarm      = 80
Pump               = 20   → overridden by any alarm
```

---

## Hysteresis

Configured per-sensor via `hysteresis` attribute (same units as value).

- Temperature threshold_hi = 50°C, hysteresis = 5°C
- Alarm turns **ON** when temp > 50°C
- Alarm turns **OFF** when temp < **45°C** (50 − 5)

---

## Delays

- `on_delay_ms`: Condition must be continuously true for this duration before rule fires
- `off_delay_ms`: After condition clears, rule stays active for this duration

---

## Python Code Generator

```bash
python3 tools/rule_engine_gen.py --config <xml_file> --out <output_dir>

Options:
  --config         Path to XML configuration file
  --out            Output directory for generated C files (default: generated/)
  --no-validate    Skip XML validation
```

### Validation checks performed:
- Duplicate sensor/output/rule/schedule IDs
- ID range violations (sensors: 1–99, outputs: 1–99)
- Invalid operator references
- Undefined sensor/output references in rules
- Undefined schedule references
- BETWEEN operator missing value_lo/value_hi
- Min/max range sanity
- Condition/action count limits (max 8 each)

---

## Building

### Linux Simulation
```bash
make          # Generate + build
make run      # Build and run
make test     # Run unit tests
make clean
```

### STM32 (STM32CubeIDE / Makefile)
```bash
# Copy platform/stm32/ files into your CubeIDE project
# Add compiler define: STM32_PLATFORM
# Add include paths: src/core, src/hal/stm32, src/scheduler, src/logger, generated/
# Add source files: src/core/re_engine.c, src/hal/stm32/hal_stm32.c, generated/*.c
```

### ESP32 (ESP-IDF)
```bash
cd platform/esp32
. $IDF_PATH/export.sh
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

---

## Extending the System

### Add a new sensor
1. Add `<Sensor>` entry to XML
2. Re-run generator: `make generate`
3. Implement HAL read in `hal_*.c` if new peripheral type

### Add a new rule
1. Add `<Rule>` entry to XML
2. Re-run generator
3. Firmware automatically evaluates the new rule — no C changes needed

### Add a new output type
1. Add to `OutputTypeEnum` in `re_types.h`
2. Add to `OUTPUT_TYPE_MAP` in generator
3. Add `<Output>` to XML

### Port to a new platform
1. Implement 8 HAL functions in a new `hal_<platform>.c`
2. Add platform `#ifdef` guards
3. Add `#define <PLATFORM>_PLATFORM` to your build system

---

## Design Constraints

| Parameter | Limit |
|-----------|-------|
| Max sensors | 99 |
| Max outputs | 99 |
| Max rules | 1000 |
| Conditions per rule | 8 |
| Actions per rule | 8 |
| Schedules | 32 |
| Slots per schedule | 8 |
| RAM log entries | 256 |
| Dynamic memory | **None (zero malloc/free)** |

---

## License

MIT License — Utkranti Yantra Pvt Ltd
