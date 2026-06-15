/**
 * @file alarm_event_mgr.c
 * @brief Alarm Event Manager implementation
 */

#include "alarm_event_mgr.h"
#include <string.h>
#include <stdio.h>

#ifndef ALARM_MAX
#define ALARM_MAX 64U
#endif

static struct {
    AlarmDesc_t         *descs[ALARM_MAX];
    AlarmRuntimeState_t *states[ALARM_MAX];
    uint8_t              count;
} s_alm;

static int8_t find_alarm(uint16_t id) {
    uint8_t i;
    for (i = 0U; i < s_alm.count; i++) if (s_alm.descs[i]->id == id) return (int8_t)i;
    return -1;
}

/* Event callback — called by event bus for any trigger event */
static void on_trigger_event(EventID_t id, const void *payload,
                              uint8_t len, void *arg)
{
    (void)len;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();

    for (i = 0U; i < s_alm.count; i++) {
        AlarmDesc_t         *desc  = s_alm.descs[i];
        AlarmRuntimeState_t *state = s_alm.states[i];

        if (!desc->enabled) continue;
        if (desc->trigger_event != id) continue;
        if (state->state == ALARM_STATE_INHIBITED) continue;

        if (state->state == ALARM_STATE_INACTIVE) {
            int32_t tval = 0;
            if (payload && len >= 4U) {
                /* AnalogReadingPayload_t: id, value_x100, ts, source */
                const AnalogReadingPayload_t *rp = (const AnalogReadingPayload_t *)payload;
                tval = rp->value_x100;
            }
            state->state         = ALARM_STATE_ACTIVE;
            state->raised_ms     = now;
            state->trigger_value = tval;
            state->raise_count++;

            AlarmPayload_t p = {
                desc->id, 0U, desc->sensor_id,
                desc->severity, tval, now
            };
            EVENT_PUBLISH(EVT_ALARM_RAISED, &p, sizeof(p));
            printf("[ALARM] RAISED  id=%u  name=%s  sev=%u  val=%ld\n",
                   desc->id, desc->name, desc->severity, (long)tval);
        }
    }
}

/* Separate subscribe per unique trigger event type */
static void maybe_subscribe(EventID_t ev) {
    /* Subscribe once per unique event ID (idempotent — bus re-uses freed slots) */
    EVENT_SUBSCRIBE(ev, on_trigger_event, NULL);
}

void AlarmEventMgr_Init(void) {
    (void)memset(&s_alm, 0, sizeof(s_alm));
}

bool AlarmEventMgr_AddAlarm(AlarmDesc_t *desc, AlarmRuntimeState_t *state) {
    if (!desc || !state || s_alm.count >= ALARM_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_alm.descs[s_alm.count]  = desc;
    s_alm.states[s_alm.count] = state;
    s_alm.count++;
    /* Subscribe to the trigger event */
    maybe_subscribe(desc->trigger_event);
    return true;
}

void AlarmEventMgr_Acknowledge(uint16_t id) {
    int8_t idx = find_alarm(id);
    if (idx < 0) return;
    AlarmRuntimeState_t *state = s_alm.states[(uint8_t)idx];
    if (state->state == ALARM_STATE_ACTIVE || state->state == ALARM_STATE_ESCALATED) {
        state->state   = ALARM_STATE_ACKNOWLEDGED;
        state->acked_ms = Scheduler_GetTick();
        AlarmPayload_t p = { id, 0U, s_alm.descs[(uint8_t)idx]->sensor_id,
                             s_alm.descs[(uint8_t)idx]->severity,
                             state->trigger_value, state->acked_ms };
        EVENT_PUBLISH(EVT_ALARM_ACKNOWLEDGED, &p, sizeof(p));
        printf("[ALARM] ACKNOWLEDGED  id=%u\n", id);
    }
}

void AlarmEventMgr_Inhibit(uint16_t id, bool inhibit) {
    int8_t idx = find_alarm(id);
    if (idx < 0) return;
    s_alm.states[(uint8_t)idx]->state = inhibit ? ALARM_STATE_INHIBITED : ALARM_STATE_INACTIVE;
    AlarmPayload_t p = { id, 0U, s_alm.descs[(uint8_t)idx]->sensor_id,
                         s_alm.descs[(uint8_t)idx]->severity, 0, Scheduler_GetTick() };
    EVENT_PUBLISH(EVT_ALARM_INHIBITED, &p, sizeof(p));
}

AlarmState_t AlarmEventMgr_GetState(uint16_t id) {
    int8_t idx = find_alarm(id);
    return (idx >= 0) ? s_alm.states[(uint8_t)idx]->state : ALARM_STATE_INACTIVE;
}

void AlarmEventMgr_Task(void *arg) {
    (void)arg;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();
    for (i = 0U; i < s_alm.count; i++) {
        AlarmDesc_t         *desc  = s_alm.descs[i];
        AlarmRuntimeState_t *state = s_alm.states[i];
        if (!desc->enabled) continue;
        if (state->state != ALARM_STATE_ACTIVE) continue;
        if (desc->ack_timeout_ms == 0U) continue;
        if ((now - state->raised_ms) >= desc->ack_timeout_ms) {
            state->state = ALARM_STATE_ESCALATED;
            AlarmPayload_t p = { desc->id, 0U, desc->sensor_id, desc->severity,
                                 state->trigger_value, now };
            EVENT_PUBLISH(EVT_ALARM_ESCALATED, &p, sizeof(p));
            printf("[ALARM] ESCALATED  id=%u  name=%s\n", desc->id, desc->name);
        }
    }
}
