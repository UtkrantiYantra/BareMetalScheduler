# Baremetal RTOS - Extended
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

## Quick start (host)
```bash
mkdir build && cd build
cmake -DTARGET_PORT=host ..
cmake --build .
./rtos
```

## Python requirements
Install PyYAML for YAML support:
```bash
pip install pyyaml
```

## Using the generator
Edit `config/system.yaml`, then run:
```bash
python3 python/generate_from_yaml.py config/system.yaml --out include/generated_config.h --platform stm32f4
```

This will emit `generated_config.h` and platform glue in `generated/`.
