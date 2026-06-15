# Requirement Document — LEDManager

**Module ID:** MOD-LED · **Source:** `src/modules/led/` · **Event base:** `0x0300`

## 1. Purpose
Drive status LEDs with three operating states — **ON**, **OFF**, **BLINK** — where
blink patterns are fully configurable (on-time, off-time, repeat count) so distinct
rates encode distinct meanings (e.g. slow = heartbeat, fast = error).

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-LED-01 | Each LED shall be statically registered via `LED_DEF(id, name, gpio, active_high)`. |
| REQ-LED-02 | The module shall support exactly three logical states: ON (solid), OFF, BLINK. |
| REQ-LED-03 | Blink shall be parameterised by `on_ms`, `off_ms`, and `repeat` (0 = continuous; N = one-shot sequence then auto-OFF). |
| REQ-LED-04 | One-shot sequences shall publish `EVT_LED_PATTERN_DONE` on completion. |
| REQ-LED-05 | Phase transitions shall be evaluated from the scheduler task using the millisecond tick — no busy waiting. |
| REQ-LED-06 | `active_high=false` shall invert the physical drive level (open-drain wiring). |

## 3. Events Published
| Event | ID | When |
|-------|----|------|
| `EVT_LED_ON` | 0x0300 | Solid ON commanded |
| `EVT_LED_OFF` | 0x0301 | OFF commanded |
| `EVT_LED_BLINK_START` | 0x0302 | Blink pattern started |
| `EVT_LED_BLINK_STOP` | 0x0303 | Blink stopped by command |
| `EVT_LED_PATTERN_DONE` | 0x0304 | One-shot sequence completed |

Payload: `LEDPayload_t { led_id, state, blink_on_ms, blink_off_ms, repeat_count, timestamp_ms }`

## 4. Events Consumed
None directly (API-driven).

## 5. Dependencies
`scheduler.h`, `event_bus.h`, weak `platform_led_write`.
