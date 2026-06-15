/**
 * @file relay_mgr.c
 * @brief Relay Manager implementation
 */

#include "relay_mgr.h"
#include "EventList.h"
#include "EventConfigRelay.h"
#include "EventConfigAlarm.h"
#include <string.h>
#include <stdio.h>

#ifndef RELAY_MAX
#define RELAY_MAX 32U
#endif

static struct {
    RelayDesc_t         *descs[RELAY_MAX];
    RelayRuntimeState_t *states[RELAY_MAX];
    uint8_t              count;
} s_relay;

/* Weak HAL write — override in platform HAL */
__attribute__((weak)) void platform_gpio_write(uint32_t pin, uint8_t level) {
    printf("[RELAY] GPIO pin %lu → %s\n", (unsigned long)pin, level ? "ON" : "OFF");
}
__attribute__((weak)) uint8_t platform_gpio_read(uint32_t pin) { (void)pin; return 0U; }

static int8_t find_relay(uint8_t id) {
    uint8_t i;
    for (i = 0U; i < s_relay.count; i++) if (s_relay.descs[i]->id == id) return (int8_t)i;
    return -1;
}

void RelayMgr_Init(void) { (void)memset(&s_relay, 0, sizeof(s_relay)); }

bool RelayMgr_AddRelay(RelayDesc_t *desc, RelayRuntimeState_t *state) {
    if (!desc || !state || s_relay.count >= RELAY_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_relay.descs[s_relay.count]  = desc;
    s_relay.states[s_relay.count] = state;
    s_relay.count++;
    return true;
}

bool RelayMgr_SetState(uint8_t relay_id, RelayState_t new_state, uint8_t priority,
                        uint16_t source_rule, uint32_t pulse_ms)
{
    int8_t idx = find_relay(relay_id);
    if (idx < 0) return false;

    RelayDesc_t         *desc  = s_relay.descs[(uint8_t)idx];
    RelayRuntimeState_t *state = s_relay.states[(uint8_t)idx];

    if (!desc->enabled) return false;

    /* Priority arbitration */
    if (new_state == RELAY_STATE_ON && priority < state->active_priority) return false;

    /* Interlock check: all bits in mask must correspond to relays that are OFF */
    if (new_state == RELAY_STATE_ON && desc->interlock_mask != 0U) {
        uint8_t i;
        for (i = 0U; i < s_relay.count; i++) {
            if ((desc->interlock_mask >> s_relay.descs[i]->id) & 1U) {
                if (s_relay.states[i]->state != RELAY_STATE_OFF) {
                    RelayPayload_t lk = { relay_id, 0U, priority, source_rule,
                                          Scheduler_GetTick() };
                    EVENT_PUBLISH(EVT_RELAY_INTERLOCK, &lk, sizeof(lk));
                    return false;
                }
            }
        }
    }

    /* Apply */
    uint8_t hw_level = (new_state == RELAY_STATE_ON || new_state == RELAY_STATE_PULSING) ? 1U : 0U;
    platform_gpio_write(desc->gpio_pin, hw_level);

    if (new_state == RELAY_STATE_PULSING) {
        state->pulse_end_ms = Scheduler_GetTick() + pulse_ms;
    }

    state->state           = new_state;
    state->actual_state    = (desc->feedback_pin != 0U)
                              ? platform_gpio_read(desc->feedback_pin) : 0xFFU;
    state->active_priority = priority;
    state->owner_rule_id   = source_rule;
    state->last_refresh_ms = Scheduler_GetTick();
    if (new_state == RELAY_STATE_ON || new_state == RELAY_STATE_PULSING) state->on_count++;

    RelayPayload_t p = { relay_id, hw_level, priority, source_rule, Scheduler_GetTick() };
    EventID_t ev = (hw_level) ? EVT_RELAY_ON : EVT_RELAY_OFF;
    EVENT_PUBLISH(ev, &p, sizeof(p));
    return true;
}

RelayState_t RelayMgr_GetState(uint8_t relay_id) {
    int8_t idx = find_relay(relay_id);
    return (idx >= 0) ? s_relay.states[(uint8_t)idx]->state : RELAY_STATE_FAULT;
}

bool RelayMgr_GetStatus(uint8_t relay_id, RelayState_t *logical_out,
                         uint8_t *actual_out)
{
    int8_t idx = find_relay(relay_id);
    if (idx < 0) return false;
    RelayDesc_t         *desc  = s_relay.descs[(uint8_t)idx];
    RelayRuntimeState_t *state = s_relay.states[(uint8_t)idx];

    if (logical_out) *logical_out = state->state;
    if (actual_out) {
        *actual_out = (desc->feedback_pin != 0U)
                       ? platform_gpio_read(desc->feedback_pin) : 0xFFU;
        state->actual_state = *actual_out;
    }
    return true;
}

void RelayMgr_Task(void *arg) {
    (void)arg;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();
    for (i = 0U; i < s_relay.count; i++) {
        RelayDesc_t         *desc  = s_relay.descs[i];
        RelayRuntimeState_t *state = s_relay.states[i];

        /* 1. Pulse expiry */
        if (state->state == RELAY_STATE_PULSING && now >= state->pulse_end_ms) {
            RelayMgr_SetState(desc->id, RELAY_STATE_OFF, 0U, 0U, 0U);
            RelayPayload_t p = { desc->id, 0U, 0U, 0U, now };
            EVENT_PUBLISH(EVT_RELAY_PULSED, &p, sizeof(p));
        }

        /* 2. Periodic refresh — re-assert GPIO to recover stuck drivers.
         *    Any glitch (EMI, brown-out on the driver, latch-up) that flipped
         *    the physical output gets corrected on the next refresh cycle. */
        if (desc->refresh_ms > 0U &&
            (now - state->last_refresh_ms) >= desc->refresh_ms) {
            uint8_t hw = (state->state == RELAY_STATE_ON ||
                          state->state == RELAY_STATE_PULSING) ? 1U : 0U;
            platform_gpio_write(desc->gpio_pin, hw);
            state->last_refresh_ms = now;

            /* 3. Feedback verification: logical vs actual */
            if (desc->feedback_pin != 0U) {
                uint8_t actual = platform_gpio_read(desc->feedback_pin);
                state->actual_state = actual;
                if (actual != hw) {
                    state->fault_count++;
                    RelayPayload_t fp = { desc->id, actual,
                                          state->active_priority,
                                          state->owner_rule_id, now };
                    EVENT_PUBLISH(EVT_RELAY_FAULT, &fp, sizeof(fp));
                }
            }
        }
    }
}
