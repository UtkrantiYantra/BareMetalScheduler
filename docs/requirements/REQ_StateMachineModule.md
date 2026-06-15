# Requirement Document — StateMachineModule

**Module ID:** MOD-SM · **Source:** `src/modules/statemachine/` · **Event base:** `0x0900`

## 1. Purpose
Reusable state-machine engine any module can instantiate from static configuration
tables. One engine supports both **flat FSM** and **hierarchical HSM** (UML
semantics: event bubbling to parents, exit/entry chains through the lowest common
ancestor).

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-SM-01 | Configuration is fully static: `SM_STATE(...)` table + `SM_TRANSITION(...)` table + `SM_MACHINE_DEF(...)`. No dynamic allocation. |
| REQ-SM-02 | FSM mode: all states have `parent = SM_NO_PARENT`. |
| REQ-SM-03 | HSM mode: states may declare a parent; an unhandled event in the current state bubbles up the parent chain. |
| REQ-SM-04 | Transitions support optional `guard` (may veto) and `action` (runs between exit and entry chains). |
| REQ-SM-05 | On transition the engine shall run exit actions from current state up to (excluding) the LCA, then entry actions down to the target. |
| REQ-SM-06 | Every successful transition shall publish `EVT_SM_TRANSITION` with machine ID, from-state, to-state, trigger. |
| REQ-SM-07 | `SM_IsInState()` shall return true for the current state or any of its ancestors (HSM containment query). |
| REQ-SM-08 | Multiple independent machine instances shall coexist (per-instance runtime in `SM_Machine_t`). |

## 3. Events Published
`EVT_SM_TRANSITION` (0x0900) · `EVT_SM_GUARD_REJECTED` (0x0901) · `EVT_SM_UNHANDLED` (0x0902)

Payload: `SMTransitionPayload_t { machine_id, from_state, to_state, trigger, timestamp_ms }`

## 4. Events Consumed
None directly — owners feed triggers via `SM_Dispatch()`, optionally from their own event-bus subscriptions.

## 5. Dependencies
`event_bus.h`, `EventList.h`.
