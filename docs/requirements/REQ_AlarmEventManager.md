# Requirement Document — AlarmEventManager

**Module ID:** MOD-ALARM · **Source:** `src/modules/alarm/` · **Event base:** `0x0400`

## 1. Purpose
Convert raw module events into managed alarm records with severity, lifecycle
(raise → acknowledge → escalate → clear), latching, and inhibit/shelve support.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-ALM-01 | Registration via `ALARM_DEF(id, name, trigger_event, sensor_id, severity, ack_timeout_ms, latching)`. |
| REQ-ALM-02 | The module shall subscribe to each alarm's `trigger_event` on registration. |
| REQ-ALM-03 | Trigger while INACTIVE → state ACTIVE, capture trigger value, publish `EVT_ALARM_RAISED`. |
| REQ-ALM-04 | Unacknowledged ACTIVE alarms past `ack_timeout_ms` → ESCALATED, publish `EVT_ALARM_ESCALATED`. |
| REQ-ALM-05 | `Acknowledge()` from ACTIVE/ESCALATED → ACKNOWLEDGED, publish `EVT_ALARM_ACKNOWLEDGED`. |
| REQ-ALM-06 | INHIBITED alarms shall ignore triggers until released. |
| REQ-ALM-07 | Severities: INFO, LOW, MEDIUM, HIGH, CRITICAL. |

## 3. Events Published
`EVT_ALARM_RAISED` (0x0400) · `EVT_ALARM_CLEARED` (0x0401) · `EVT_ALARM_ACKNOWLEDGED` (0x0402) · `EVT_ALARM_ESCALATED` (0x0403) · `EVT_ALARM_INHIBITED` (0x0404) · `EVT_ALARM_SHELVED` (0x0405) · `EVT_ALARM_LOG_OVERFLOW` (0x0406)

Payload: `AlarmPayload_t { alarm_id, source_rule_id, sensor_id, severity, trigger_value, timestamp_ms }`

## 4. Events Consumed
Any event configured as a `trigger_event` — typically `EVT_ANALOG_THRESHOLD_HIGH/LOW`, `EVT_DIGITAL_RISING_EDGE`.

## 5. Dependencies
`scheduler.h`, `event_bus.h`, EventConfig headers of producer modules.
