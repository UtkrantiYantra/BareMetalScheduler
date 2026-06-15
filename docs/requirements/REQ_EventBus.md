# Requirement Document — EventBus (Core)

**Module ID:** CORE-EVT · **Source:** `src/event/` · **Tooling:** `tools/fw_gen.py`

## 1. Purpose
Decoupled publish/subscribe backbone. Modules publish typed events into an
ISR-safe ring; the dispatcher delivers to subscribers in task context.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-EVT-01 | Each module owns an `EventConfigXXX.h` declaring its `#define EVT_*` IDs (unique base per module) and payload structs. |
| REQ-EVT-02 | At build time `fw_gen.py` merges all `EventConfig*.h` into `generated/EventList.h` and **fails the build on ID collisions**. |
| REQ-EVT-03 | `EventBus_Publish` shall be ISR-safe (lock-free SPSC ring, payload copied ≤ `EVENT_MAX_PAYLOAD`). |
| REQ-EVT-04 | `EventBus_Dispatch(max)` drains the ring in task context and fans out to all matching subscribers. |
| REQ-EVT-05 | Ring-full publishes shall be counted as drops and return false. |
| REQ-EVT-06 | Macros: `EVENT_SUBSCRIBE`, `EVENT_PUBLISH`, `EVENT_SIGNAL` (zero payload). |
| REQ-EVT-07 | Advanced payload path: the imported `baremetal_rtos` event registry + mempool provides zero-copy (`EVT_FLAG_ZERO_COPY`) and per-subscriber copy semantics for large payloads. |

## 3. Module Event-ID Map
| Module | Base | File |
|--------|------|------|
| AnalogSensor | 0x0100 | EventConfigAnalogSensor.h |
| Relay | 0x0200 | EventConfigRelay.h |
| LED | 0x0300 | EventConfigLED.h |
| Alarm | 0x0400 | EventConfigAlarm.h |
| DigitalSensor | 0x0500 | EventConfigDigitalSensor.h |
| Logging | 0x0600 | EventConfigLogging.h |
| Buzzer | 0x0700 | EventConfigBuzzer.h |
| Queue | 0x0800 | EventConfigQueue.h |
| StateMachine | 0x0900 | EventConfigStateMachine.h |
| TOU | 0x0A00 | EventConfigTOU.h |
| Diagnostics | 0x0B00 | EventConfigDiagnostics.h |
