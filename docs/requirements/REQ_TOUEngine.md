# Requirement Document — TOUEngine

**Module ID:** MOD-TOU · **Source:** `src/modules/tou/` · **Event base:** `0x0A00`

## 1. Purpose
Standalone time-of-use window evaluation (independent of the rule engine's internal
schedule gates) so any module can gate behaviour on tariff/time windows, with edge
events at window start/end.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-TOU-01 | Definition via `TOU_SCHEDULE_DEF(id, name, start_h, start_m, end_h, end_m, day_mask)`. |
| REQ-TOU-02 | Overnight windows (end < start) shall wrap midnight correctly. |
| REQ-TOU-03 | `day_mask` bit0=Sunday … bit6=Saturday; 0x7F = all days. |
| REQ-TOU-04 | `TOU_IsActive(id)` evaluates immediately against the RTC via `HAL_GetRTC`. |
| REQ-TOU-05 | The periodic task shall detect window edges and publish `EVT_TOU_WINDOW_START` / `EVT_TOU_WINDOW_END` exactly once per edge. |

## 3. Events Published
`EVT_TOU_WINDOW_START` (0x0A00) · `EVT_TOU_WINDOW_END` (0x0A01)

## 4. Events Consumed
None.

## 5. Dependencies
`re_types.h`/`re_hal.h` (RTC), `event_bus.h`, `scheduler.h`.
