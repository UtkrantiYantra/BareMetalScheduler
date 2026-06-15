# Requirement Document — DiagnosticsSubsystem

**Module ID:** MOD-DIAG · **Source:** `src/modules/diag/` · **Event base:** `0x0B00`

## 1. Purpose
Aggregate health/statistics snapshots from all subsystems (scheduler, event bus,
rule engine, logging) into one structure for dump, telemetry, or watchdog policy.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-DIA-01 | `Diag_Collect(&snap)` gathers: scheduler ticks/dispatches/overruns, event-bus published/dispatched/dropped, rule-engine loops/evals/fired/errors, logging written/dropped. |
| REQ-DIA-02 | `Diag_Print()` renders a human-readable snapshot. |
| REQ-DIA-03 | The periodic task shall publish `EVT_DIAG_SNAPSHOT` at its configured interval and `EVT_DIAG_ANOMALY` if drop/overrun counters increased since the previous snapshot. |

## 3. Events Published
`EVT_DIAG_SNAPSHOT` (0x0B00) · `EVT_DIAG_ANOMALY` (0x0B01)

## 4. Events Consumed
None (pulls statistics via subsystem getter APIs).

## 5. Dependencies
`scheduler.h`, `event_bus.h`, `re_engine.h`, `logging_mgr.h`.
