# Baremetal RTOS — Extended Firmware Framework

Includes:
- Priority-based event-driven scheduler
- Multiple memory pools (zero-copy)
- ISR-safe event posting
- Timer wheel
- Power-save idle
- CMake build
- Python tools (YAML config, code generator)
- Logging module
- Profiling hooks
- Platform ports: STM32F4 / ESP32 / AVR / Host (stubs)

Plus the integrated rule-engine framework:
- Configuration-driven rule engine (XML → C tables)
- Bare-metal cooperative scheduler + OSAL (bare-metal / FreeRTOS / ThreadX)
- Publish/subscribe event bus with build-time generated EventList.h
- Modules: Relay, LED, Buzzer, DigitalSensor, AnalogSensor, Alarm, Logging,
  Queue, StateMachine (FSM + HSM), TOU, Diagnostics

## Quick start (host)
```bash
mkdir build_cmake && cd build_cmake
cmake -DTARGET_PORT=host ..
cmake --build .
./rule_engine_sim
```

Or with the classic Makefile:
```bash
make run    # build + run simulation
make test   # 28 unit tests
```

## Python requirements
```bash
pip install pyyaml
```

## Generators
```bash
# Merge per-module EventConfig*.h into generated/EventList.h + EventRegistry
python3 tools/fw_gen.py --event-dir src/event/configs --out generated/

# XML rules → C tables
python3 tools/rule_engine_gen.py --config xml/examples/industrial_automation.xml --out generated/

# YAML system config → header
python3 python/generate_from_yaml.py config/system.yaml --out include/generated_config.h
```

## Module requirement documents
See `docs/requirements/REQ_<Module>.md` — one per module, covering functionality,
events consumed, and events published.

## License
GPL-3.0 (see LICENSE).
