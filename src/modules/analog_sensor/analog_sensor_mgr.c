/**
 * @file analog_sensor_mgr.c
 * @brief Analog Sensor Manager implementation
 *
 * Source dispatch table:
 *  ADC    → analog_read_adc()    — platform HAL call
 *  I2C    → analog_read_i2c()    — platform HAL call
 *  SPI    → analog_read_spi()    — platform HAL call
 *  RS485  → analog_read_rs485()  — Modbus RTU single-register read
 *  UART   → analog_read_uart()   — last frame injected via _uart_inject()
 *  INJECT → injected value table (test / Linux sim)
 */

#include "analog_sensor_mgr.h"
#include "EventConfigAnalogSensor.h"
#include "re_hal.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */
#ifndef ANALOG_MAX_CHANNELS
#define ANALOG_MAX_CHANNELS  32U
#endif

/* =========================================================================
 * Private state
 * ========================================================================= */
static struct {
    AnalogChannelDesc_t  *descs[ANALOG_MAX_CHANNELS];
    AnalogChannelState_t *states[ANALOG_MAX_CHANNELS];
    uint8_t               count;

    /* Injection table (all sources, for INJECT or simulation) */
    int32_t  injected[256];      /* indexed by sensor_id */
    bool     injected_valid[256];
} s_mgr;

/* =========================================================================
 * Platform read stubs
 * Implement these per-platform in src/hal/<platform>/analog_hal.c
 * Signature: bool platform_analog_read_<source>(uint32_t hw_addr, int32_t *raw_out)
 * Returns true on success, false on error.
 * ========================================================================= */
__attribute__((weak)) bool platform_adc_read(uint32_t ch, int32_t *raw)   { *raw = 0; return true; }
__attribute__((weak)) bool platform_i2c_read(uint32_t addr, int32_t *raw) { *raw = 0; return true; }
__attribute__((weak)) bool platform_spi_read(uint32_t cs,   int32_t *raw) { *raw = 0; return true; }
__attribute__((weak)) bool platform_rs485_read(uint32_t reg, int32_t *raw){ *raw = 0; return true; }
__attribute__((weak)) bool platform_uart_read(uint32_t idx, int32_t *raw) { *raw = 0; return true; }

/* =========================================================================
 * Source dispatch
 * ========================================================================= */
static bool read_raw(const AnalogChannelDesc_t *desc, int32_t *raw_out)
{
    switch (desc->source)
    {
        case ANALOG_SRC_ADC:    return platform_adc_read(desc->hw_addr,   raw_out);
        case ANALOG_SRC_I2C:    return platform_i2c_read(desc->hw_addr,   raw_out);
        case ANALOG_SRC_SPI:    return platform_spi_read(desc->hw_addr,   raw_out);
        case ANALOG_SRC_RS485:  return platform_rs485_read(desc->hw_addr, raw_out);
        case ANALOG_SRC_UART:   return platform_uart_read(desc->hw_addr,  raw_out);
        case ANALOG_SRC_INJECT:
        default:
            /* Return injected value if available, else 0 */
            if (s_mgr.injected_valid[desc->id]) {
                /* Injected value is already scaled × 100, skip scale step */
                *raw_out = s_mgr.injected[desc->id];
                return true;
            }
            *raw_out = 0;
            return false;
    }
}

/* =========================================================================
 * Scale raw → value × 100
 * ========================================================================= */
static int32_t apply_scale(const AnalogChannelDesc_t *desc, int32_t raw)
{
    if (desc->source == ANALOG_SRC_INJECT) {
        /* Already scaled by caller */
        return raw;
    }
    if (desc->scale_den == 0) return raw;
    return (raw * desc->scale_num) / desc->scale_den + desc->offset_x100;
}

/* =========================================================================
 * Init
 * ========================================================================= */
void AnalogSensorMgr_Init(void)
{
    (void)memset(&s_mgr, 0, sizeof(s_mgr));
}

/* =========================================================================
 * Add channel
 * ========================================================================= */
bool AnalogSensorMgr_AddChannel(AnalogChannelDesc_t *desc, AnalogChannelState_t *state)
{
    if (!desc || !state) return false;
    if (s_mgr.count >= ANALOG_MAX_CHANNELS) return false;

    (void)memset(state, 0, sizeof(*state));
    s_mgr.descs[s_mgr.count]  = desc;
    s_mgr.states[s_mgr.count] = state;
    s_mgr.count++;
    return true;
}

/* =========================================================================
 * Software injection
 * ========================================================================= */
void AnalogSensorMgr_Inject(uint8_t sensor_id, int32_t value_x100)
{
    s_mgr.injected[sensor_id]       = value_x100;
    s_mgr.injected_valid[sensor_id] = true;

    /* Also push into the rule engine HAL so rules evaluate immediately */
#if defined(LINUX_PLATFORM)
    extern void RE_HAL_SimSetSensor(uint8_t id, int32_t v);
    RE_HAL_SimSetSensor(sensor_id, value_x100);
#endif
}

/* =========================================================================
 * Scheduler task
 * ========================================================================= */
void AnalogSensorMgr_Task(void *arg)
{
    (void)arg;
    uint8_t   i;
    uint32_t  now_ms = Scheduler_GetTick();

    for (i = 0U; i < s_mgr.count; i++) {
        AnalogChannelDesc_t  *desc  = s_mgr.descs[i];
        AnalogChannelState_t *state = s_mgr.states[i];

        if (!desc->enabled) continue;

        /* Period check */
        if ((now_ms - state->last_sample_ms) < desc->sample_ms) continue;
        state->last_sample_ms = now_ms;

        /* Read */
        int32_t raw = 0;
        bool ok = read_raw(desc, &raw);

        if (!ok) {
            state->error_count++;
            state->fault = true;
            AnalogFaultPayload_t fp = {
                desc->id,
                (uint8_t)state->error_count,
                now_ms
            };
            EVENT_PUBLISH(EVT_ANALOG_SENSOR_FAULT, &fp, sizeof(fp));
            continue;
        }

        /* Scale */
        int32_t val = apply_scale(desc, raw);
        state->last_value_x100 = val;
        state->sample_count++;

        if (state->fault) {
            state->fault = false;
            state->error_count = 0U;
            AnalogReadingPayload_t rp = { desc->id, val, now_ms, (uint8_t)desc->source };
            EVENT_PUBLISH(EVT_ANALOG_SENSOR_OK, &rp, sizeof(rp));
        }

        /* Push into rule engine sensor table */
#if defined(LINUX_PLATFORM)
        extern void RE_HAL_SimSetSensor(uint8_t id, int32_t v);
        RE_HAL_SimSetSensor(desc->id, val);
#endif

        /* Publish reading event */
        AnalogReadingPayload_t rp = { desc->id, val, now_ms, (uint8_t)desc->source };
        EVENT_PUBLISH(EVT_ANALOG_READING_READY, &rp, sizeof(rp));

        /* Threshold checks */
        if (desc->hi_thresh_x100 != 0) {
            if (!state->threshold_hi_active && val >= desc->hi_thresh_x100) {
                state->threshold_hi_active = true;
                EVENT_PUBLISH(EVT_ANALOG_THRESHOLD_HIGH, &rp, sizeof(rp));
            } else if (state->threshold_hi_active && val < desc->hi_thresh_x100) {
                state->threshold_hi_active = false;
            }
        }

        if (desc->lo_thresh_x100 != 0) {
            if (!state->threshold_lo_active && val <= desc->lo_thresh_x100) {
                state->threshold_lo_active = true;
                EVENT_PUBLISH(EVT_ANALOG_THRESHOLD_LOW, &rp, sizeof(rp));
            } else if (state->threshold_lo_active && val > desc->lo_thresh_x100) {
                state->threshold_lo_active = false;
            }
        }
    }
}

/* =========================================================================
 * Query API
 * ========================================================================= */
bool AnalogSensorMgr_GetValue(uint8_t sensor_id, int32_t *out)
{
    uint8_t i;
    for (i = 0U; i < s_mgr.count; i++) {
        if (s_mgr.descs[i]->id == sensor_id) {
            if (out) *out = s_mgr.states[i]->last_value_x100;
            return true;
        }
    }
    return false;
}

void AnalogSensorMgr_SetEnabled(uint8_t sensor_id, bool enabled)
{
    uint8_t i;
    for (i = 0U; i < s_mgr.count; i++) {
        if (s_mgr.descs[i]->id == sensor_id) {
            s_mgr.descs[i]->enabled = enabled;
            return;
        }
    }
}
