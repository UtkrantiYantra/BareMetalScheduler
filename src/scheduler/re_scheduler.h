/**
 * @file re_scheduler.h
 * @brief TOU scheduler interface (external table consumed by engine)
 *
 * Schedule evaluation is performed inside re_engine.c via engine_check_schedule().
 * This header exposes the schedule table symbols declared in rule_table.c.
 */

#ifndef RE_SCHEDULER_H
#define RE_SCHEDULER_H

#include "re_types.h"

/* Declared in rule_table.c (generated) */
extern const ScheduleDescriptor_t g_schedule_table[];
extern const uint16_t             RE_NUM_SCHEDULES;

/**
 * @brief Check whether a named schedule is currently active.
 *
 * Exposed for unit tests and manual queries.
 *
 * @param[in] schedule_id  ID from schedule table
 * @param[in] rtc          Current RTC time
 * @return true if schedule slot is active
 */
bool RE_Scheduler_IsActive(uint8_t schedule_id, const RE_RTC_t *rtc);

#endif /* RE_SCHEDULER_H */
