/**
 * @file tou_engine.c
 * @brief Time-Of-Use engine implementation
 */

#include "tou_engine.h"
#include "EventConfigTOU.h"
#include "re_hal.h"
#include "scheduler.h"
#include <string.h>

#ifndef TOU_MAX
#define TOU_MAX 16U
#endif

static struct {
    TOUSchedule_t     *scheds[TOU_MAX];
    TOURuntimeState_t *states[TOU_MAX];
    uint8_t            count;
} s_tou;

void TOU_Init(void) { (void)memset(&s_tou, 0, sizeof(s_tou)); }

bool TOU_AddSchedule(TOUSchedule_t *sched, TOURuntimeState_t *state)
{
    if (!sched || !state || s_tou.count >= TOU_MAX) return false;
    (void)memset(state, 0, sizeof(*state));
    s_tou.scheds[s_tou.count]  = sched;
    s_tou.states[s_tou.count] = state;
    s_tou.count++;
    return true;
}

static bool eval_window(const TOUSchedule_t *s, const RE_RTC_t *rtc)
{
    uint16_t now_min   = (uint16_t)(rtc->hour * 60U + rtc->minute);
    uint16_t start_min = (uint16_t)(s->start_hour * 60U + s->start_min);
    uint16_t end_min   = (uint16_t)(s->end_hour   * 60U + s->end_min);

    /* Day-of-week gate */
    if (((s->day_mask >> rtc->weekday) & 1U) == 0U) {
        /* For overnight spans, the window that started yesterday may still
         * be running — accept if yesterday's bit is set and we are before end */
        if (end_min < start_min) {
            uint8_t yesterday = (uint8_t)((rtc->weekday + 6U) % 7U);
            if (((s->day_mask >> yesterday) & 1U) != 0U && now_min < end_min)
                return true;
        }
        return false;
    }

    if (end_min >= start_min) {
        return (now_min >= start_min) && (now_min < end_min);
    }
    /* Overnight span: active if after start OR before end */
    return (now_min >= start_min) || (now_min < end_min);
}

bool TOU_IsActive(uint8_t schedule_id)
{
    uint8_t i;
    RE_RTC_t rtc;
    if (HAL_GetRTC(&rtc) != RE_OK) return false;
    for (i = 0U; i < s_tou.count; i++) {
        if (s_tou.scheds[i]->id == schedule_id) {
            return s_tou.scheds[i]->enabled && eval_window(s_tou.scheds[i], &rtc);
        }
    }
    return false;
}

void TOU_Task(void *arg)
{
    (void)arg;
    uint8_t i;
    RE_RTC_t rtc;
    if (HAL_GetRTC(&rtc) != RE_OK) return;

    for (i = 0U; i < s_tou.count; i++) {
        TOUSchedule_t     *s  = s_tou.scheds[i];
        TOURuntimeState_t *st = s_tou.states[i];
        if (!s->enabled) continue;

        bool active = eval_window(s, &rtc);
        if (active != st->active) {
            st->active = active;
            TOUPayload_t p = { s->id, active ? 1U : 0U, Scheduler_GetTick() };
            EVENT_PUBLISH(active ? EVT_TOU_WINDOW_START : EVT_TOU_WINDOW_END,
                          &p, sizeof(p));
        }
    }
}
