/**
 * @file scheduler.h
 * @brief Bare-metal cooperative scheduler
 *
 * Features
 * --------
 *  - Task table with period-based dispatch (SCHED_TASK_DEF macro)
 *  - Millisecond tick counter (driven by SysTick or platform timer)
 *  - Software timer pool (wraps OSAL_Timer_t)
 *  - Run-to-completion cooperative model — no stack switching
 *
 * Usage
 * -----
 * 1. Define tasks with SCHED_TASK_DEF in your module .c files.
 * 2. Register them with Scheduler_AddTask() during init.
 * 3. Call Scheduler_Tick() from SysTick ISR (1 ms).
 * 4. Call Scheduler_Run() in your main loop — it dispatches due tasks.
 *
 * Defining a task with SCHED_TASK_DEF:
 *
 *   SCHED_TASK_DEF(RelayMgr_Task, "relay", 10U, 3U);   // 10 ms period, prio 3
 *   // Then in init:
 *   Scheduler_AddTask(&RelayMgr_Task, RelayMgr_Run, NULL);
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "osal.h"

/* =========================================================================
 * Configuration constants (override via compiler flags)
 * ========================================================================= */
#ifndef SCHED_MAX_TASKS
#define SCHED_MAX_TASKS     32U
#endif

#ifndef SCHED_MAX_TIMERS
#define SCHED_MAX_TIMERS    16U
#endif

/* =========================================================================
 * Task entry
 * ========================================================================= */
typedef void (*Sched_TaskFn_t)(void *arg);

typedef struct {
    const char     *name;
    Sched_TaskFn_t  fn;
    void           *arg;
    uint32_t        period_ms;   /* 0 = run every tick */
    uint8_t         priority;    /* lower number = higher priority */
    uint32_t        last_run_ms;
    uint32_t        run_count;
    uint32_t        overrun_count;
    bool            enabled;
} Sched_Task_t;

/* =========================================================================
 * SCHED_TASK_DEF — declare + statically initialise a task descriptor
 *
 * Example:
 *   SCHED_TASK_DEF(g_relay_task, "relay", 10U, 3U);
 * ========================================================================= */
#define SCHED_TASK_DEF(var, name_, period_ms_, prio_)   \
    static Sched_Task_t var = {                          \
        (name_),                                         \
        NULL,          /* fn set by Scheduler_AddTask */ \
        NULL,          /* arg */                         \
        (period_ms_),                                    \
        (prio_),                                         \
        0U,            /* last_run_ms */                 \
        0U,            /* run_count */                   \
        0U,            /* overrun_count */               \
        true           /* enabled */                     \
    }

/* =========================================================================
 * Scheduler control
 * ========================================================================= */

/** @brief Initialise scheduler. Call once before adding tasks. */
void Scheduler_Init(void);

/**
 * @brief Register a task.
 * @param task   Pointer to a static Sched_Task_t (use SCHED_TASK_DEF).
 * @param fn     Task function pointer.
 * @param arg    Opaque argument passed to fn.
 * @return true on success, false if table is full.
 */
bool Scheduler_AddTask(Sched_Task_t *task, Sched_TaskFn_t fn, void *arg);

/** @brief Remove a task by pointer. */
void Scheduler_RemoveTask(Sched_Task_t *task);

/** @brief Enable or disable a task without removing it. */
void Scheduler_SetTaskEnabled(Sched_Task_t *task, bool enabled);

/**
 * @brief Advance the tick counter. Call from SysTick ISR every 1 ms.
 * Also ticks all registered software timers.
 */
void Scheduler_Tick(void);

/**
 * @brief Cooperative run loop. Call from main(). Never returns.
 * Dispatches tasks whose period has elapsed, in priority order.
 */
void Scheduler_Run(void);

/** @brief Return current uptime in milliseconds. */
uint32_t Scheduler_GetTick(void);

/* =========================================================================
 * Software timer convenience API (wraps OSAL_Timer_t)
 * ========================================================================= */

/**
 * @brief Register a software timer.
 * @param timer      Pointer to a static OSAL_Timer_t (use OSAL_TIMER_DEF).
 * @param period_ms  Timer period.
 * @param repeat     true = periodic, false = one-shot.
 * @param cb         Callback invoked on expiry.
 * @param arg        Argument passed to cb.
 * @return true on success.
 */
bool Scheduler_AddTimer(OSAL_Timer_t *timer, uint32_t period_ms, bool repeat,
                        OSAL_TimerCb_t cb, void *arg);

/** @brief Start a registered timer. */
void Scheduler_StartTimer(OSAL_Timer_t *timer);

/** @brief Stop a registered timer. */
void Scheduler_StopTimer(OSAL_Timer_t *timer);

/** @brief Restart a timer from zero. */
void Scheduler_ResetTimer(OSAL_Timer_t *timer);

/* =========================================================================
 * Diagnostics
 * ========================================================================= */
typedef struct {
    uint32_t tick_count;
    uint32_t task_dispatches;
    uint32_t timer_fires;
    uint32_t total_overruns;
} Sched_Diag_t;

void Scheduler_GetDiag(Sched_Diag_t *out);

#endif /* SCHEDULER_H */
