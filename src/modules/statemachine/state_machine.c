/**
 * @file state_machine.c
 * @brief State Machine implementation (flat FSM + hierarchical HSM)
 */

#include "state_machine.h"
#include "EventConfigStateMachine.h"
#include "scheduler.h"
#include <string.h>

#define SM_MAX_DEPTH 8U   /* max hierarchy nesting */

/* =========================================================================
 * Helpers
 * ========================================================================= */
static const SM_State_t *find_state(const SM_Machine_t *m, SM_StateID_t id)
{
    uint16_t i;
    if (id == SM_NO_STATE || id == SM_NO_PARENT) return NULL;
    for (i = 0U; i < m->state_count; i++)
        if (m->states[i].id == id) return &m->states[i];
    return NULL;
}

/** Build ancestor path root→state. Returns depth (path[0] = root). */
static uint8_t build_path(const SM_Machine_t *m, SM_StateID_t id,
                          const SM_State_t *path[SM_MAX_DEPTH])
{
    const SM_State_t *chain[SM_MAX_DEPTH];
    uint8_t n = 0U, i;
    const SM_State_t *s = find_state(m, id);
    while (s && n < SM_MAX_DEPTH) {
        chain[n++] = s;
        s = find_state(m, s->parent);
    }
    /* Reverse: root first */
    for (i = 0U; i < n; i++) path[i] = chain[n - 1U - i];
    return n;
}

/* =========================================================================
 * Init — run entry chain from root to initial state
 * ========================================================================= */
bool SM_Init(SM_Machine_t *m)
{
    const SM_State_t *path[SM_MAX_DEPTH];
    uint8_t depth, i;

    if (!m || !find_state(m, m->initial)) return false;

    depth = build_path(m, m->initial, path);
    for (i = 0U; i < depth; i++) {
        if (path[i]->on_entry) path[i]->on_entry(m->ctx);
    }
    m->current     = m->initial;
    m->initialized = true;
    return true;
}

/* =========================================================================
 * Dispatch — HSM event bubbling + LCA-based exit/entry sequencing
 * ========================================================================= */
bool SM_Dispatch(SM_Machine_t *m, uint16_t trigger)
{
    const SM_Transition_t *t = NULL;
    SM_StateID_t search;
    uint16_t i;

    if (!m || !m->initialized) return false;

    /* 1. Search current state, then bubble up parent chain */
    search = m->current;
    while (search != SM_NO_PARENT && search != SM_NO_STATE) {
        for (i = 0U; i < m->trans_count; i++) {
            if (m->transitions[i].from == search &&
                m->transitions[i].trigger == trigger) {
                t = &m->transitions[i];
                break;
            }
        }
        if (t) break;
        const SM_State_t *s = find_state(m, search);
        search = s ? s->parent : SM_NO_PARENT;
    }

    if (!t) {
        SMTransitionPayload_t p = { m->id, m->current, SM_NO_STATE, trigger,
                                     Scheduler_GetTick() };
        EVENT_PUBLISH(EVT_SM_UNHANDLED_EVENT, &p, sizeof(p));
        return false;
    }

    /* 2. Guard check */
    if (t->guard && !t->guard(m->ctx)) {
        SMTransitionPayload_t p = { m->id, m->current, t->to, trigger,
                                     Scheduler_GetTick() };
        EVENT_PUBLISH(EVT_SM_GUARD_REJECTED, &p, sizeof(p));
        return false;
    }

    /* 3. Compute LCA between current and target via root paths */
    {
        const SM_State_t *src_path[SM_MAX_DEPTH], *dst_path[SM_MAX_DEPTH];
        uint8_t src_d = build_path(m, m->current, src_path);
        uint8_t dst_d = build_path(m, t->to,      dst_path);
        uint8_t lca   = 0U;
        int8_t  j;
        uint8_t k;

        while (lca < src_d && lca < dst_d && src_path[lca] == dst_path[lca]) lca++;
        /* lca = number of shared ancestors from root */

        /* 4. Exit chain: current up to (not including) LCA */
        for (j = (int8_t)(src_d - 1U); j >= (int8_t)lca; j--) {
            if (src_path[j]->on_exit) src_path[j]->on_exit(m->ctx);
        }

        /* 5. Transition action */
        if (t->action) t->action(m->ctx);

        /* 6. Entry chain: LCA down to target */
        for (k = lca; k < dst_d; k++) {
            if (dst_path[k]->on_entry) dst_path[k]->on_entry(m->ctx);
        }
    }

    /* 7. Commit + publish */
    {
        SMTransitionPayload_t p = { m->id, m->current, t->to, trigger,
                                     Scheduler_GetTick() };
        m->current = t->to;
        m->transition_count++;
        EVENT_PUBLISH(EVT_SM_TRANSITION, &p, sizeof(p));
    }
    return true;
}

/* =========================================================================
 * Query API
 * ========================================================================= */
SM_StateID_t SM_GetState(const SM_Machine_t *m)
{
    return m ? m->current : SM_NO_STATE;
}

const char *SM_GetStateName(const SM_Machine_t *m)
{
    const SM_State_t *s = m ? find_state(m, m->current) : NULL;
    return s ? s->name : "?";
}

bool SM_IsInState(const SM_Machine_t *m, SM_StateID_t state)
{
    SM_StateID_t cur;
    if (!m) return false;
    cur = m->current;
    while (cur != SM_NO_PARENT && cur != SM_NO_STATE) {
        if (cur == state) return true;
        const SM_State_t *s = find_state(m, cur);
        cur = s ? s->parent : SM_NO_PARENT;
    }
    return false;
}

void SM_ForceState(SM_Machine_t *m, SM_StateID_t state)
{
    const SM_State_t *s;
    if (!m) return;
    s = find_state(m, state);
    if (!s) return;
    m->current = state;
    if (s->on_entry) s->on_entry(m->ctx);
}
