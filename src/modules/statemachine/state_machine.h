/**
 * @file state_machine.h
 * @brief State Machine Module — FSM and Hierarchical State Machine (HSM)
 *
 * Any module can instantiate one or more state machines from static
 * configuration tables.  Two modes in one engine:
 *
 *   FSM — flat: every state has parent == NULL.
 *   HSM — hierarchical: states may have a parent state.  If the current
 *         state has no transition for an event, the parent chain is searched
 *         (event bubbling).  On transition, exit actions run from the current
 *         state up to (not including) the lowest common ancestor, then entry
 *         actions run down to the target state — standard UML HSM semantics.
 *
 * Configuration is fully static — no dynamic allocation:
 *
 *   // 1. State table
 *   static const SM_State_t k_states[] = {
 *       SM_STATE(ST_OPERATIONAL, "operational", SM_NO_PARENT, on_op_entry, NULL),
 *       SM_STATE(ST_IDLE,        "idle",        ST_OPERATIONAL, NULL, NULL),
 *       SM_STATE(ST_RUNNING,     "running",     ST_OPERATIONAL, on_run_entry, on_run_exit),
 *       SM_STATE(ST_FAULT,       "fault",       SM_NO_PARENT, on_fault_entry, NULL),
 *   };
 *
 *   // 2. Transition table
 *   static const SM_Transition_t k_trans[] = {
 *       SM_TRANSITION(ST_IDLE,        EVT_START_CMD,  ST_RUNNING, guard_ok, do_start),
 *       SM_TRANSITION(ST_RUNNING,     EVT_STOP_CMD,   ST_IDLE,    NULL,     do_stop),
 *       SM_TRANSITION(ST_OPERATIONAL, EVT_FAULT_DET,  ST_FAULT,   NULL,     NULL),
 *       //            ^ defined on parent: fires from ANY child of OPERATIONAL
 *   };
 *
 *   // 3. Machine
 *   SM_MACHINE_DEF(g_motor_sm, 1, "motor", k_states, k_trans, ST_IDLE, &my_ctx);
 *
 *   // 4. Run
 *   SM_Init(&g_motor_sm);
 *   SM_Dispatch(&g_motor_sm, EVT_START_CMD);   // ST_IDLE → ST_RUNNING
 *
 * Each successful transition publishes EVT_SM_TRANSITION on the event bus.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include "event_bus.h"
#include "EventList.h"

#define SM_NO_PARENT   0xFFFFU
#define SM_NO_STATE    0xFFFFU

typedef uint16_t SM_StateID_t;

/* User callbacks */
typedef void (*SM_ActionFn_t)(void *ctx);
typedef bool (*SM_GuardFn_t)(void *ctx);

/* =========================================================================
 * State descriptor
 * ========================================================================= */
typedef struct {
    SM_StateID_t  id;
    const char   *name;
    SM_StateID_t  parent;     /**< SM_NO_PARENT for top-level (flat FSM)     */
    SM_ActionFn_t on_entry;   /**< NULL = none                               */
    SM_ActionFn_t on_exit;    /**< NULL = none                               */
} SM_State_t;

#define SM_STATE(id_, name_, parent_, entry_, exit_) \
    { (id_), (name_), (parent_), (entry_), (exit_) }

/* =========================================================================
 * Transition descriptor
 * ========================================================================= */
typedef struct {
    SM_StateID_t  from;       /**< Source state (may be a parent for HSM)    */
    uint16_t      trigger;    /**< Event value that fires this transition    */
    SM_StateID_t  to;
    SM_GuardFn_t  guard;      /**< NULL = always allowed                     */
    SM_ActionFn_t action;     /**< Runs between exit and entry chains        */
} SM_Transition_t;

#define SM_TRANSITION(from_, trigger_, to_, guard_, action_) \
    { (from_), (trigger_), (to_), (guard_), (action_) }

/* =========================================================================
 * Machine instance
 * ========================================================================= */
typedef struct {
    uint8_t                 id;
    const char             *name;
    const SM_State_t       *states;
    uint16_t                state_count;
    const SM_Transition_t  *transitions;
    uint16_t                trans_count;
    SM_StateID_t            initial;
    void                   *ctx;          /**< Opaque user context           */

    /* Runtime */
    SM_StateID_t            current;
    uint32_t                transition_count;
    bool                    initialized;
} SM_Machine_t;

#define SM_MACHINE_DEF(var, id_, name_, states_, trans_, initial_, ctx_)  \
    static SM_Machine_t var = {                                            \
        (id_), (name_),                                                    \
        (states_), (uint16_t)(sizeof(states_)/sizeof((states_)[0])),     \
        (trans_),  (uint16_t)(sizeof(trans_)/sizeof((trans_)[0])),       \
        (initial_), (ctx_),                                                \
        SM_NO_STATE, 0U, false                                             \
    }

/* =========================================================================
 * API
 * ========================================================================= */

/** @brief Enter the initial state (runs full entry chain from root). */
bool SM_Init(SM_Machine_t *m);

/**
 * @brief Dispatch an event to the machine.
 * Searches the current state, then parent chain (HSM bubbling), for a
 * transition whose trigger matches.  Runs guard, exit chain, action,
 * entry chain. Publishes EVT_SM_TRANSITION on success.
 * @return true if a transition fired, false if the event was unhandled or
 *         rejected by a guard.
 */
bool SM_Dispatch(SM_Machine_t *m, uint16_t trigger);

/** @brief Current state ID. */
SM_StateID_t SM_GetState(const SM_Machine_t *m);

/** @brief Current state name (or "?" if uninitialised). */
const char *SM_GetStateName(const SM_Machine_t *m);

/** @brief True if the machine is in `state` or any descendant of it (HSM). */
bool SM_IsInState(const SM_Machine_t *m, SM_StateID_t state);

/** @brief Force machine to a specific state (test/recovery; runs entry only). */
void SM_ForceState(SM_Machine_t *m, SM_StateID_t state);

#endif /* STATE_MACHINE_H */
