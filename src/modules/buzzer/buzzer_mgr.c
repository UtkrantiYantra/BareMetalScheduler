/**
 * @file buzzer_mgr.c
 * @brief Buzzer Manager implementation
 */

#include "buzzer_mgr.h"
#include "EventConfigBuzzer.h"
#include <string.h>
#include <stdio.h>

#ifndef BUZZER_MAX
#define BUZZER_MAX 8U
#endif

static struct {
    BuzzerDesc_t         *descs[BUZZER_MAX];
    BuzzerRuntimeState_t *states[BUZZER_MAX];
    uint8_t               count;
} s_bz;

__attribute__((weak)) void platform_buzzer_write(uint32_t pin, uint8_t level) {
    printf("[BUZZER] pin %lu -> %s\n", (unsigned long)pin, level ? "ON" : "OFF");
}

static int8_t find_buzzer(uint8_t id) {
    uint8_t i;
    for (i = 0U; i < s_bz.count; i++) if (s_bz.descs[i]->id == id) return (int8_t)i;
    return -1;
}

static void apply_hw(const BuzzerDesc_t *d, bool on) {
    uint8_t level = on ? (d->active_high ? 1U : 0U) : (d->active_high ? 0U : 1U);
    platform_buzzer_write(d->gpio_pin, level);
}

void BuzzerMgr_Init(void) { (void)memset(&s_bz, 0, sizeof(s_bz)); }

bool BuzzerMgr_AddBuzzer(BuzzerDesc_t *desc, BuzzerRuntimeState_t *state) {
    if (!desc || !state || s_bz.count >= BUZZER_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_bz.descs[s_bz.count]  = desc;
    s_bz.states[s_bz.count] = state;
    s_bz.count++;
    return true;
}

void BuzzerMgr_SetSolid(uint8_t id, bool on) {
    int8_t idx = find_buzzer(id);
    if (idx < 0) return;
    BuzzerRuntimeState_t *st = s_bz.states[(uint8_t)idx];
    st->mode = on ? BUZZER_MODE_ON : BUZZER_MODE_OFF;
    apply_hw(s_bz.descs[(uint8_t)idx], on);
    BuzzerPayload_t p = { id, on ? 1U : 0U, 0U, 0U, 0U, Scheduler_GetTick() };
    EVENT_PUBLISH(on ? EVT_BUZZER_ON : EVT_BUZZER_OFF, &p, sizeof(p));
}

void BuzzerMgr_SetBeep(uint8_t id, uint16_t on_ms, uint16_t off_ms, uint8_t repeat) {
    int8_t idx = find_buzzer(id);
    if (idx < 0) return;
    BuzzerRuntimeState_t *st = s_bz.states[(uint8_t)idx];
    st->mode             = (repeat == 0U) ? BUZZER_MODE_BEEP : BUZZER_MODE_ONESHOT;
    st->on_ms            = on_ms;
    st->off_ms           = off_ms;
    st->repeat_count     = repeat;
    st->repeat_remaining = repeat;
    st->phase_on         = true;
    st->phase_end_ms     = Scheduler_GetTick() + on_ms;
    apply_hw(s_bz.descs[(uint8_t)idx], true);
    BuzzerPayload_t p = { id, 1U, on_ms, off_ms, repeat, Scheduler_GetTick() };
    EVENT_PUBLISH(EVT_BUZZER_BEEP_START, &p, sizeof(p));
}

void BuzzerMgr_Off(uint8_t id) { BuzzerMgr_SetSolid(id, false); }

BuzzerMode_t BuzzerMgr_GetMode(uint8_t id) {
    int8_t idx = find_buzzer(id);
    return (idx >= 0) ? s_bz.states[(uint8_t)idx]->mode : BUZZER_MODE_OFF;
}

void BuzzerMgr_Task(void *arg) {
    (void)arg;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();
    for (i = 0U; i < s_bz.count; i++) {
        BuzzerRuntimeState_t *st = s_bz.states[i];
        BuzzerDesc_t         *d  = s_bz.descs[i];
        if (st->mode != BUZZER_MODE_BEEP && st->mode != BUZZER_MODE_ONESHOT) continue;
        if (now < st->phase_end_ms) continue;

        st->phase_on = !st->phase_on;
        apply_hw(d, st->phase_on);
        st->phase_end_ms = now + (st->phase_on ? st->on_ms : st->off_ms);

        if (!st->phase_on && st->mode == BUZZER_MODE_ONESHOT) {
            if (st->repeat_remaining > 0U) st->repeat_remaining--;
            if (st->repeat_remaining == 0U) {
                st->mode = BUZZER_MODE_OFF;
                apply_hw(d, false);
                BuzzerPayload_t p = { d->id, 0U, 0U, 0U, 0U, now };
                EVENT_PUBLISH(EVT_BUZZER_PATTERN_DONE, &p, sizeof(p));
            }
        }
    }
}
