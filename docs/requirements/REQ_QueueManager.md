# Requirement Document — QueueManager

**Module ID:** MOD-QUEUE · **Source:** `src/modules/queue/` · **Event base:** `0x0800`

## 1. Purpose
Named-queue registry layered on OSAL queues: central creation, lookup by ID or
name, occupancy statistics, high-water-mark supervision, and overflow events —
identical API on bare-metal, FreeRTOS, and ThreadX.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-QUE-01 | Static definition via `QUEUE_MGR_DEF(id, name, item_size, depth, hw_pct)` (allocates OSAL storage, zero heap). |
| REQ-QUE-02 | Registration/lookup: `Register`, `FindById`, `FindByName`. |
| REQ-QUE-03 | `Send` on a full queue shall return false, count the drop, and publish `EVT_QUEUE_OVERFLOW`. |
| REQ-QUE-04 | Occupancy crossing `hw_pct` (high-water percentage) shall publish `EVT_QUEUE_HIGH_WATER` once per excursion. |
| REQ-QUE-05 | Per-queue statistics: sends, receives, drops, high-water mark; dumped via `PrintStats`. |
| REQ-QUE-06 | Underlying primitive is `OSAL_Queue_t`, so RTOS backend swaps require no module change. |

## 3. Events Published
`EVT_QUEUE_OVERFLOW` (0x0800) · `EVT_QUEUE_HIGH_WATER` (0x0801) · `EVT_QUEUE_EMPTY` (0x0802)

## 4. Events Consumed
None.

## 5. Dependencies
`osal.h`, `event_bus.h`.
