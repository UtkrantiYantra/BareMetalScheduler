/**
 * @file led_mgr.c
 * @brief LED Manager implementation
 */

#include "led_mgr.h"
#include "EventConfigLED.h"
#include <string.h>
#include <stdio.h>

#ifndef LED_MAX
#define LED_MAX 32U
#endif

static struct {
    LEDDesc_t         *descs[LED_MAX];
    LEDRuntimeState_t *states[LED_MAX];
    uint8_t            count;
} s_led;

__attribute__((weak)) void platform_led_write(uint32_t pin, uint8_t level) {
    printf("[LED] pin %lu → %s\n", (unsigned long)pin, level ? "ON" : "OFF");
}

static int8_t find_led(uint8_t id) {
    uint8_t i;
    for (i = 0U; i < s_led.count; i++) if (s_led.descs[i]->id == id) return (int8_t)i;
    return -1;
}

static void apply_hw(const LEDDesc_t *d, bool on) {
    uint8_t level = on ? (d->active_high ? 1U : 0U) : (d->active_high ? 0U : 1U);
    platform_led_write(d->gpio_pin, level);
}

void LEDMgr_Init(void) { (void)memset(&s_led, 0, sizeof(s_led)); }

bool LEDMgr_AddLED(LEDDesc_t *desc, LEDRuntimeState_t *state) {
    if (!desc || !state || s_led.count >= LED_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_led.descs[s_led.count]  = desc;
    s_led.states[s_led.count] = state;
    s_led.count++;
    return true;
}

void LEDMgr_SetSolid(uint8_t id, bool on) {
    int8_t idx = find_led(id);
    if (idx < 0) return;
    LEDRuntimeState_t *st = s_led.states[(uint8_t)idx];
    st->state = on ? LED_MODE_ON : LED_MODE_OFF;
    apply_hw(s_led.descs[(uint8_t)idx], on);
    LEDPayload_t p = { id, on ? 1U : 0U, 0U, 0U, 0U, Scheduler_GetTick() };
    EVENT_PUBLISH(on ? EVT_LED_ON : EVT_LED_OFF, &p, sizeof(p));
}

void LEDMgr_SetBlink(uint8_t id, uint16_t on_ms, uint16_t off_ms, uint8_t repeat) {
    int8_t idx = find_led(id);
    if (idx < 0) return;
    LEDRuntimeState_t *st = s_led.states[(uint8_t)idx];
    st->state              = (repeat == 0U) ? LED_MODE_BLINK : LED_MODE_ONESHOT;
    st->on_ms              = on_ms;
    st->off_ms             = off_ms;
    st->repeat_count       = repeat;
    st->repeat_remaining   = repeat;
    st->phase_on           = true;
    st->phase_end_ms       = Scheduler_GetTick() + on_ms;
    apply_hw(s_led.descs[(uint8_t)idx], true);
    LEDPayload_t p = { id, 1U, on_ms, off_ms, repeat, Scheduler_GetTick() };
    EVENT_PUBLISH(EVT_LED_BLINK_START, &p, sizeof(p));
}

void LEDMgr_SetBlinkRate(uint8_t id, uint16_t on_ms, uint16_t off_ms) {
    int8_t idx = find_led(id);
    if (idx < 0) return;
    LEDRuntimeState_t *st = s_led.states[(uint8_t)idx];
    if (st->state != LED_MODE_BLINK && st->state != LED_MODE_ONESHOT) return;
    st->on_ms  = on_ms;
    st->off_ms = off_ms;   /* takes effect at the next phase boundary */
}

void LEDMgr_Off(uint8_t id) { LEDMgr_SetSolid(id, false); }

LEDMode_t LEDMgr_GetMode(uint8_t id) {
    int8_t idx = find_led(id);
    return (idx >= 0) ? s_led.states[(uint8_t)idx]->state : LED_MODE_OFF;
}

void LEDMgr_Task(void *arg) {
    (void)arg;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();
    for (i = 0U; i < s_led.count; i++) {
        LEDRuntimeState_t *st = s_led.states[i];
        LEDDesc_t         *d  = s_led.descs[i];
        if (st->state != LED_MODE_BLINK && st->state != LED_MODE_ONESHOT) continue;
        if (now < st->phase_end_ms) continue;

        st->phase_on = !st->phase_on;
        apply_hw(d, st->phase_on);
        st->phase_end_ms = now + (st->phase_on ? st->on_ms : st->off_ms);

        if (!st->phase_on && st->state == LED_MODE_ONESHOT) {
            if (st->repeat_remaining > 0U) st->repeat_remaining--;
            if (st->repeat_remaining == 0U) {
                st->state = LED_MODE_OFF;
                apply_hw(d, false);
                LEDPayload_t p = { d->id, 0U, 0U, 0U, 0U, now };
                EVENT_PUBLISH(EVT_LED_PATTERN_DONE, &p, sizeof(p));
            }
        }
    }
}
