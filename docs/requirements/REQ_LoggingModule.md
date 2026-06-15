# Requirement Document — LoggingModule

**Module ID:** MOD-LOG · **Source:** `src/modules/logging/` · **Event base:** `0x0600`

## 1. Purpose
Central typed logging with a RAM ring buffer, multiple pluggable output handlers
(backends), per-handler filtering, and typed reader APIs for selective retrieval.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-LOG-01 | Capture object types: TEXT, SENSOR, EVENT, ALARM, RELAY, DIAG, RAW — each with a typed payload union. |
| REQ-LOG-02 | Handler backends: UART, RTT, FLASH ring, SD/FatFS, CALLBACK (user function, e.g. cloud forwarder). |
| REQ-LOG-03 | Each handler filters by minimum severity AND a type bitmask. |
| REQ-LOG-04 | UART/RTT/CALLBACK dispatch immediately; FLASH/SD dispatch deferred from the scheduler task. |
| REQ-LOG-05 | Ring overflow shall overwrite oldest and publish `EVT_LOG_BUFFER_FULL`. |
| REQ-LOG-06 | Typed readers: `ReadAll(filter)`, `ReadSensor(id)`, `ReadAlarm(id)`, `ReadText(tag_prefix)` — iterator-callback pattern. |
| REQ-LOG-07 | Convenience macros: `LOG_TEXT`, `LOG_SENSOR`, `LOG_ALARM`, `LOG_EVENT`, `LOG_RELAY`. |

## 3. Events Published
`EVT_LOG_ENTRY_WRITTEN` (0x0600) · `EVT_LOG_BUFFER_FULL` (0x0601) · `EVT_LOG_HANDLER_ERROR` (0x0602) · `EVT_LOG_FLUSH_DONE` (0x0603)

## 4. Events Consumed
None directly; other modules call LOG_* macros or a subscriber forwards bus events into the log.

## 5. Dependencies
`scheduler.h`, `event_bus.h`; weak platform writers `platform_log_uart/rtt/flash/sd`.
