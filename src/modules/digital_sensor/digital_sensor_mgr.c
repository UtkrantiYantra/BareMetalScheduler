/**
 * @file digital_sensor_mgr.c
 * @brief Digital Sensor Manager implementation
 */

#include "digital_sensor_mgr.h"
#include "EventConfigDigitalSensor.h"
#include <string.h>
#include <stdio.h>

#ifndef DIGITAL_MAX
#define DIGITAL_MAX 32U
#endif

static struct {
    DigitalSensorDesc_t  *descs[DIGITAL_MAX];
    DigitalSensorState_t *states[DIGITAL_MAX];
    uint8_t               count;
    uint8_t               injected[256];
    bool                  injected_valid[256];
} s_dig;

__attribute__((weak)) uint8_t platform_gpio_read_dig(uint32_t pin) { (void)pin; return 0U; }

static int8_t find_sensor(uint8_t id) {
    uint8_t i;
    for (i = 0U; i < s_dig.count; i++) if (s_dig.descs[i]->id == id) return (int8_t)i;
    return -1;
}

void DigitalSensorMgr_Init(void) { (void)memset(&s_dig, 0, sizeof(s_dig)); }

bool DigitalSensorMgr_AddSensor(DigitalSensorDesc_t *desc, DigitalSensorState_t *state) {
    if (!desc || !state || s_dig.count >= DIGITAL_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_dig.descs[s_dig.count]  = desc;
    s_dig.states[s_dig.count] = state;
    s_dig.count++;
    return true;
}

void DigitalSensorMgr_Inject(uint8_t id, uint8_t state) {
    s_dig.injected[id]       = state;
    s_dig.injected_valid[id] = true;
}

uint8_t DigitalSensorMgr_GetState(uint8_t id) {
    int8_t idx = find_sensor(id);
    return (idx >= 0) ? s_dig.states[(uint8_t)idx]->stable_state : 0U;
}

void DigitalSensorMgr_Task(void *arg) {
    (void)arg;
    uint8_t i;
    uint32_t now = Scheduler_GetTick();

    for (i = 0U; i < s_dig.count; i++) {
        DigitalSensorDesc_t  *desc  = s_dig.descs[i];
        DigitalSensorState_t *state = s_dig.states[i];
        if (!desc->enabled) continue;

        /* Read raw */
        uint8_t raw;
        if (s_dig.injected_valid[desc->id]) {
            raw = s_dig.injected[desc->id];
        } else {
            uint8_t gpio = platform_gpio_read_dig(desc->gpio_pin);
            raw = desc->active_high ? gpio : (uint8_t)(!gpio);
        }

        if (raw != state->raw_state) {
            state->raw_state    = raw;
            state->change_time_ms = now;
            state->debouncing   = true;
        }

        if (state->debouncing && (now - state->change_time_ms) >= desc->debounce_ms) {
            state->debouncing = false;
            if (raw != state->stable_state) {
                uint8_t prev = state->stable_state;
                state->stable_state = raw;
                state->edge_count++;

                /* Push into rule engine */
#if defined(LINUX_PLATFORM)
                extern void RE_HAL_SimSetSensor(uint8_t id, int32_t v);
                RE_HAL_SimSetSensor(desc->id, (int32_t)raw);
#endif
                DigitalEdgePayload_t p = { desc->id, raw, desc->debounce_ms, now };
                EventID_t ev = (raw > prev) ? EVT_DIGITAL_RISING_EDGE : EVT_DIGITAL_FALLING_EDGE;
                EVENT_PUBLISH(ev, &p, sizeof(p));
                EVENT_PUBLISH(EVT_DIGITAL_DEBOUNCE_DONE, &p, sizeof(p));
            }
        }
    }
}
