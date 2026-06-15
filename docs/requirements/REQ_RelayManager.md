# Requirement Document — RelayManager

**Module ID:** MOD-RELAY · **Source:** `src/modules/relay/` · **Event base:** `0x0200`

## 1. Purpose
Manage discrete relay outputs with priority arbitration, hardware interlocks, pulse
control, periodic refresh (stuck-relay recovery), and logical-vs-actual state
verification through optional feedback inputs.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-RLY-01 | Each relay shall be statically registered via `RELAY_DEF(id, name, gpio, fb_pin, interlock_mask, min_prio, refresh_ms)`. |
| REQ-RLY-02 | A relay shall only turn ON if the commanding priority ≥ the priority that currently owns the output. |
| REQ-RLY-03 | A relay shall not turn ON while any relay in its `interlock_mask` is not OFF; the blocked command shall publish `EVT_RELAY_INTERLOCK`. |
| REQ-RLY-04 | `RELAY_STATE_PULSING` shall auto-revert to OFF after `pulse_ms`, publishing `EVT_RELAY_PULSED`. |
| REQ-RLY-05 | When `refresh_ms > 0`, the task shall re-assert the commanded GPIO level every `refresh_ms` so a transient driver/hardware glitch self-heals. |
| REQ-RLY-06 | `RelayMgr_GetStatus()` shall report both the **logical** (commanded) state and the **actual** state read from the feedback pin (`0xFF` if no feedback pin configured). |
| REQ-RLY-07 | A logical/actual mismatch detected during the task cycle shall increment `fault_count` and publish `EVT_RELAY_FAULT`. |

## 3. Events Published
| Event | ID | Payload | When |
|-------|----|---------|------|
| `EVT_RELAY_ON` | 0x0200 | `RelayPayload_t` | Output asserted ON |
| `EVT_RELAY_OFF` | 0x0201 | `RelayPayload_t` | Output asserted OFF |
| `EVT_RELAY_PULSED` | 0x0202 | `RelayPayload_t` | Pulse window expired |
| `EVT_RELAY_FAULT` | 0x0203 | `RelayPayload_t` | Feedback mismatch (stuck relay) |
| `EVT_RELAY_INTERLOCK` | 0x0204 | `RelayPayload_t` | ON command blocked by interlock |

## 4. Events Consumed
None directly. Commands arrive via API calls (typically from rule engine actions or AlarmEventManager handlers).

## 5. Dependencies
`scheduler.h` (tick), `event_bus.h`, `EventList.h`, platform `platform_gpio_write/read` (weak, override per target).
