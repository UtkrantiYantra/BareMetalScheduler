# Requirement Document — DigitalSensorManager

**Module ID:** MOD-DIGSEN · **Source:** `src/modules/digital_sensor/` · **Event base:** `0x0500`

## 1. Purpose
Read binary inputs (switches, detectors, E-stop, door contacts) with software
debounce, edge detection, and automatic injection of the debounced state into the
rule-engine sensor table.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-DIG-01 | Registration via `DIGITAL_SENSOR_DEF(id, name, gpio, debounce_ms, active_high)`; `id` matches rule-engine sensor IDs. |
| REQ-DIG-02 | A raw state change shall only be accepted after remaining stable for `debounce_ms`. |
| REQ-DIG-03 | Accepted transitions shall publish `EVT_DIGITAL_RISING_EDGE` or `EVT_DIGITAL_FALLING_EDGE` plus `EVT_DIGITAL_DEBOUNCE_DONE`. |
| REQ-DIG-04 | The debounced state shall be pushed into the rule engine (`RE_HAL_SimSetSensor` on Linux; real HAL path on targets). |
| REQ-DIG-05 | `DigitalSensorMgr_Inject(id, state)` shall override the GPIO read for simulation/test. |

## 3. Events Published
`EVT_DIGITAL_RISING_EDGE` (0x0500) · `EVT_DIGITAL_FALLING_EDGE` (0x0501) · `EVT_DIGITAL_STATE_ON` (0x0502) · `EVT_DIGITAL_STATE_OFF` (0x0503) · `EVT_DIGITAL_DEBOUNCE_DONE` (0x0504) · `EVT_DIGITAL_FAULT` (0x0505)

Payload: `DigitalEdgePayload_t { sensor_id, state, debounce_ms, timestamp_ms }`

## 4. Events Consumed
None.

## 5. Dependencies
`scheduler.h`, `event_bus.h`, `re_hal.h`, weak `platform_gpio_read_dig`.
