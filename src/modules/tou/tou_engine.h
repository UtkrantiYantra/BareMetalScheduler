/**
 * @file tou_engine.h
 * @brief Time-Of-Use engine — standalone schedule window evaluation
 *
 * Wraps RTC-based schedule windows so any module can gate behaviour on a
 * time-of-use window without going through the rule engine.  Supports
 * overnight spans (end < start wraps midnight) and weekly day masks.
 *
 * Publishes EVT_TOU_WINDOW_START / EVT_TOU_WINDOW_END on edge transitions
 * when TOU_Task runs periodically.
 */

#ifndef TOU_ENGINE_H
#define TOU_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "re_types.h"     /* RE_RTC_t */
#include "event_bus.h"
#include "EventList.h"

typedef struct {
    uint8_t   id;
    const char *name;
    uint8_t   start_hour, start_min;
    uint8_t   end_hour,   end_min;
    uint8_t   day_mask;     /**< bit0=Sun .. bit6=Sat; 0x7F = every day      */
    bool      enabled;
} TOUSchedule_t;

typedef struct {
    bool active;            /**< window currently active                     */
} TOURuntimeState_t;

#define TOU_SCHEDULE_DEF(var, id_, name_, sh_, sm_, eh_, em_, days_) \
    static TOUSchedule_t var = {                                       \
        (id_), (name_), (sh_), (sm_), (eh_), (em_), (days_), true     \
    }

void TOU_Init(void);
bool TOU_AddSchedule(TOUSchedule_t *sched, TOURuntimeState_t *state);

/** Evaluate window now (reads RTC via HAL). */
bool TOU_IsActive(uint8_t schedule_id);

/** Periodic task: detects window start/end edges and publishes events. */
void TOU_Task(void *arg);

#endif /* TOU_ENGINE_H */
