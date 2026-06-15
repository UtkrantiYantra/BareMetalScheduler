#define _POSIX_C_SOURCE 200809L
/**
 * @file hal_linux.c
 * @brief Linux simulation HAL implementation for the Rule Engine
 *
 * Provides software-simulated sensor readings and stdout-based output driving.
 * Suitable for CI testing and development on Linux/macOS.
 */

#include "re_hal.h"
#include "re_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/* =========================================================================
 * Log storage (RAM ring buffer for Linux sim)
 * ========================================================================= */
#define LINUX_LOG_SIZE  (512U)

static RE_LogEntry_t s_log[LINUX_LOG_SIZE];
static uint16_t      s_log_head  = 0U;
static uint16_t      s_log_tail  = 0U;
static uint16_t      s_log_count = 0U;

/* =========================================================================
 * Simulated sensor values (set by test harness via RE_HAL_SimSetSensor)
 * ========================================================================= */
static SensorValue_t s_sim_sensors[100];  /* index = sensor_id */

/**
 * @brief Test helper: inject a simulated sensor value.
 */
void RE_HAL_SimSetSensor(uint8_t sensor_id, SensorValue_t value)
{
    if (sensor_id < 100U)
    {
        s_sim_sensors[sensor_id] = value;
    }
}

/**
 * @brief Force the engine to re-read all sensors on the next tick.
 *
 * Clears last_sampled_ts for every sensor so that the sample-interval
 * guard does not prevent a read within the same wall-clock second.
 * Test-harness use only.
 */
void RE_HAL_SimForceResample(void)
{
    RE_InvalidateSensorCache();
}

/* =========================================================================
 * HAL_RE_Init
 * ========================================================================= */
RE_Status_t HAL_RE_Init(void)
{
    (void)memset(s_sim_sensors, 0, sizeof(s_sim_sensors));
    (void)memset(s_log, 0, sizeof(s_log));
    s_log_head  = 0U;
    s_log_tail  = 0U;
    s_log_count = 0U;
    RE_LOG_INFO("%s", "Linux HAL initialised (simulation mode)");
    return RE_OK;
}

void HAL_RE_DeInit(void)
{
    RE_LOG_INFO("%s", "Linux HAL deinitialised");
}

/* =========================================================================
 * HAL_ReadSensor
 * ========================================================================= */
RE_Status_t HAL_ReadSensor(uint8_t sensor_id, SensorValue_t *value)
{
    if ((sensor_id == 0U) || (sensor_id >= 100U) || (value == NULL))
    {
        return RE_ERR_INVALID_ID;
    }
    *value = s_sim_sensors[sensor_id];
    return RE_OK;
}

/* =========================================================================
 * HAL_WriteOutput
 * ========================================================================= */
RE_Status_t HAL_WriteOutput(uint8_t output_id, uint8_t state)
{
    RE_LOG_INFO("OUTPUT[%u] -> %s", output_id, (state != 0U) ? "ON" : "OFF");
    return RE_OK;
}

/* =========================================================================
 * HAL_GetRTC
 * ========================================================================= */
RE_Status_t HAL_GetRTC(RE_RTC_t *rtc)
{
    time_t     now;
    struct tm *t;

    if (rtc == NULL)
    {
        return RE_ERR_NULL_PTR;
    }

    now = time(NULL);
    t   = localtime(&now);

    rtc->hour    = (uint8_t)t->tm_hour;
    rtc->minute  = (uint8_t)t->tm_min;
    rtc->second  = (uint8_t)t->tm_sec;
    rtc->day     = (uint8_t)t->tm_mday;
    rtc->month   = (uint8_t)(t->tm_mon + 1);
    rtc->year    = (uint16_t)(t->tm_year + 1900);
    rtc->weekday = (uint8_t)t->tm_wday;

    return RE_OK;
}

/* =========================================================================
 * HAL_GetTimestamp
 * ========================================================================= */
RE_Timestamp_t HAL_GetTimestamp(void)
{
    return (RE_Timestamp_t)time(NULL);
}

/* =========================================================================
 * HAL_GetMicros
 * ========================================================================= */
uint32_t HAL_GetMicros(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(((uint64_t)ts.tv_sec * 1000000UL) + ((uint64_t)ts.tv_nsec / 1000UL));
}

/* =========================================================================
 * HAL_LogEvent
 * ========================================================================= */
RE_Status_t HAL_LogEvent(const RE_LogEntry_t *entry)
{
    if (entry == NULL)
    {
        return RE_ERR_NULL_PTR;
    }

    /* Print to stdout */
    printf("[LOG] ts=%lu rule=%u sensor=%u val=%ld output=%u action=%u\n",
           (unsigned long)entry->timestamp,
           entry->rule_id,
           entry->sensor_id,
           (long)entry->sensor_value,
           entry->output_id,
           (unsigned int)entry->action);

    /* Store in RAM ring buffer */
    s_log[s_log_head] = *entry;
    s_log_head = (uint16_t)((s_log_head + 1U) % LINUX_LOG_SIZE);
    if (s_log_count < LINUX_LOG_SIZE)
    {
        s_log_count++;
    }
    else
    {
        s_log_tail = (uint16_t)((s_log_tail + 1U) % LINUX_LOG_SIZE);
    }

    return RE_OK;
}

/* =========================================================================
 * HAL_ReadLog
 * ========================================================================= */
RE_Status_t HAL_ReadLog(RE_LogEntry_t *buf, uint16_t count, uint16_t *read)
{
    uint16_t i;
    uint16_t idx;
    uint16_t to_read;

    if ((buf == NULL) || (read == NULL))
    {
        return RE_ERR_NULL_PTR;
    }

    to_read = (count < s_log_count) ? count : s_log_count;
    *read   = to_read;

    for (i = 0U; i < to_read; i++)
    {
        idx    = (uint16_t)((s_log_tail + i) % LINUX_LOG_SIZE);
        buf[i] = s_log[idx];
    }

    return RE_OK;
}

void HAL_ClearLog(void)
{
    (void)memset(s_log, 0, sizeof(s_log));
    s_log_head  = 0U;
    s_log_tail  = 0U;
    s_log_count = 0U;
}

/* =========================================================================
 * Watchdog / Critical Section (no-op on Linux)
 * ========================================================================= */
void HAL_KickWatchdog(void)
{
    /* No watchdog on Linux simulation */
}

void HAL_EnterCritical(void)
{
    /* Single-threaded simulation: no-op */
}

void HAL_ExitCritical(void)
{
    /* Single-threaded simulation: no-op */
}
