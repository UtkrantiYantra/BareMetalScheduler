# Requirement Document — Scheduler & OSAL (Core)

**Module ID:** CORE-SCHED · **Source:** `src/scheduler/`, `src/osal/`

## 1. Purpose
Bare-metal cooperative scheduler (task table, 1 ms tick, software timers) plus an
OS abstraction layer so identical module code runs on bare-metal, FreeRTOS, or
ThreadX.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-SCH-01 | Tasks declared via `SCHED_TASK_DEF(name, period_ms, priority)`; registered with `Scheduler_AddTask` (priority-ordered, lower number first). |
| REQ-SCH-02 | `Scheduler_Tick()` from a 1 ms ISR advances uptime and ticks all software timers. |
| REQ-SCH-03 | `Scheduler_Run()` dispatches due tasks run-to-completion; overruns are counted per task. |
| REQ-SCH-04 | Software timers: one-shot or periodic, callback-based, via `Scheduler_AddTimer/Start/Stop/Reset`. |
| REQ-SCH-05 | OSAL primitives — Queue, Mutex, Timer, Task-create macro — compile to bare-metal inline code by default, or FreeRTOS (`OSAL_FREERTOS`) / ThreadX (`OSAL_THREADX`) calls, with zero changes in module code. |
| REQ-SCH-06 | The imported timer wheel (`src/baremetal_rtos/timerwheel.c`) provides O(1) bucket timers that publish events on expiry, for high-count timer workloads. |

## 3. Events Published / Consumed
None directly (infrastructure).
