# Requirement Document — BuzzerManager

**Module ID:** MOD-BUZZER · **Source:** `src/modules/buzzer/` · **Event base:** `0x0700`

## 1. Purpose
Audible-indicator counterpart of LEDManager: OFF / ON (continuous tone) / BEEP
patterns with configurable rates, so cadence encodes severity (slow beep = warning,
fast beep = critical, N-beep one-shot = acknowledge chirp).

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-BUZ-01 | Registration via `BUZZER_DEF(id, name, gpio, active_high)`. |
| REQ-BUZ-02 | Modes: `OFF`, `ON`, `BEEP` (continuous pattern), `ONESHOT` (N repeats then auto-OFF). |
| REQ-BUZ-03 | `BuzzerMgr_SetBeep(id, on_ms, off_ms, repeat)` with `repeat=0` meaning continuous. |
| REQ-BUZ-04 | One-shot completion shall publish `EVT_BUZZER_PATTERN_DONE`. |
| REQ-BUZ-05 | Behaviour and timing semantics shall mirror LEDManager exactly (shared mental model). |

## 3. Events Published
| Event | ID | When |
|-------|----|------|
| `EVT_BUZZER_ON` | 0x0700 | Continuous tone on |
| `EVT_BUZZER_OFF` | 0x0701 | Silenced |
| `EVT_BUZZER_BEEP_START` | 0x0702 | Pattern started |
| `EVT_BUZZER_PATTERN_DONE` | 0x0703 | One-shot sequence done |

## 4. Events Consumed
None directly; typically commanded by AlarmEventManager subscribers.

## 5. Dependencies
`scheduler.h`, `event_bus.h`, weak `platform_buzzer_write`.
