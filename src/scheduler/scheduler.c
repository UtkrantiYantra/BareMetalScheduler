/**
 * @file scheduler.c
 * @brief Bare-metal cooperative scheduler implementation
 */

#include "scheduler.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Private state
 * ========================================================================= */
static struct {
    Sched_Task_t  *tasks[SCHED_MAX_TASKS];
    uint8_t        task_count;

    OSAL_Timer_t  *timers[SCHED_MAX_TIMERS];
    uint8_t        timer_count;

    volatile uint32_t tick_ms;      /* uptime counter, incremented in Scheduler_Tick */

    Sched_Diag_t   diag;
    bool           initialized;
} s_sched;

/* =========================================================================
 * Init
 * ========================================================================= */
void Scheduler_Init(void)
{
    (void)memset(&s_sched, 0, sizeof(s_sched));
    s_sched.initialized = true;
}

/* =========================================================================
 * Task management
 * ========================================================================= */
bool Scheduler_AddTask(Sched_Task_t *task, Sched_TaskFn_t fn, void *arg)
{
    uint8_t i;
    if (!task || !fn) return false;
    if (s_sched.task_count >= SCHED_MAX_TASKS) return false;

    /* Check not already registered */
    for (i = 0U; i < s_sched.task_count; i++) {
        if (s_sched.tasks[i] == task) return true; /* idempotent */
    }

    task->fn          = fn;
    task->arg         = arg;
    task->last_run_ms = s_sched.tick_ms;

    /* Insert in priority order (lower number = higher priority, runs first) */
    uint8_t pos = s_sched.task_count;
    for (i = 0U; i < s_sched.task_count; i++) {
        if (task->priority < s_sched.tasks[i]->priority) {
            pos = i; break;
        }
    }
    /* Shift right to make room */
    for (i = s_sched.task_count; i > pos; i--) {
        s_sched.tasks[i] = s_sched.tasks[i - 1U];
    }
    s_sched.tasks[pos] = task;
    s_sched.task_count++;
    return true;
}

void Scheduler_RemoveTask(Sched_Task_t *task)
{
    uint8_t i, j;
    for (i = 0U; i < s_sched.task_count; i++) {
        if (s_sched.tasks[i] == task) {
            for (j = i; j < s_sched.task_count - 1U; j++) {
                s_sched.tasks[j] = s_sched.tasks[j + 1U];
            }
            s_sched.task_count--;
            return;
        }
    }
}

void Scheduler_SetTaskEnabled(Sched_Task_t *task, bool enabled)
{
    if (task) task->enabled = enabled;
}

/* =========================================================================
 * Tick — called from SysTick ISR or platform 1-ms timer
 * ========================================================================= */
void Scheduler_Tick(void)
{
    uint8_t i;
    s_sched.tick_ms++;
    s_sched.diag.tick_count++;

    /* Tick all registered software timers */
    for (i = 0U; i < s_sched.timer_count; i++) {
        OSAL_Timer_Tick(s_sched.timers[i], s_sched.tick_ms);
    }
}

/* =========================================================================
 * Run loop — cooperative dispatcher
 * ========================================================================= */
void Scheduler_Run(void)
{
    uint8_t  i;
    uint32_t now;

    while (true) {
        now = s_sched.tick_ms;

        for (i = 0U; i < s_sched.task_count; i++) {
            Sched_Task_t *t = s_sched.tasks[i];

            if (!t->enabled || !t->fn) continue;

            if (t->period_ms == 0U ||
                (now - t->last_run_ms) >= t->period_ms)
            {
                uint32_t start = s_sched.tick_ms;
                t->fn(t->arg);
                uint32_t elapsed = s_sched.tick_ms - start;

                t->last_run_ms = now;
                t->run_count++;
                s_sched.diag.task_dispatches++;

                /* Overrun: task took longer than its own period */
                if (t->period_ms > 0U && elapsed > t->period_ms) {
                    t->overrun_count++;
                    s_sched.diag.total_overruns++;
                }
            }
        }
    }
}

/* =========================================================================
 * Tick accessor
 * ========================================================================= */
uint32_t Scheduler_GetTick(void)
{
    return s_sched.tick_ms;
}

/* =========================================================================
 * Software timer registration
 * ========================================================================= */
bool Scheduler_AddTimer(OSAL_Timer_t *timer, uint32_t period_ms, bool repeat,
                        OSAL_TimerCb_t cb, void *arg)
{
    if (!timer) return false;
    if (s_sched.timer_count >= SCHED_MAX_TIMERS) return false;

    OSAL_Timer_Init(timer, period_ms, repeat, cb, arg);
    OSAL_Timer_SetExpiry(timer, s_sched.tick_ms);
    s_sched.timers[s_sched.timer_count++] = timer;
    return true;
}

void Scheduler_StartTimer(OSAL_Timer_t *timer)
{
    if (timer) {
        OSAL_Timer_SetExpiry(timer, s_sched.tick_ms);
        OSAL_Timer_Start(timer);
    }
}

void Scheduler_StopTimer(OSAL_Timer_t *timer)
{
    if (timer) OSAL_Timer_Stop(timer);
}

void Scheduler_ResetTimer(OSAL_Timer_t *timer)
{
    if (timer) {
        OSAL_Timer_SetExpiry(timer, s_sched.tick_ms);
        OSAL_Timer_Reset(timer);
    }
}

/* =========================================================================
 * Diagnostics
 * ========================================================================= */
void Scheduler_GetDiag(Sched_Diag_t *out)
{
    if (out) *out = s_sched.diag;
}
