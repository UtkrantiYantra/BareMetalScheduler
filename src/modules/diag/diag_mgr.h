/**
 * @file diag_mgr.h
 * @brief Diagnostics subsystem — aggregates health data from all framework
 *        components into one snapshot, prints it, and publishes
 *        EVT_DIAG_SNAPSHOT / EVT_DIAG_HEALTH_WARNING.
 *
 * Sources: scheduler, event bus, rule engine, logging module, queues.
 */

#ifndef DIAG_MGR_H
#define DIAG_MGR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t uptime_ms;

    /* Scheduler */
    uint32_t sched_ticks;
    uint32_t sched_dispatches;
    uint32_t sched_overruns;

    /* Event bus */
    uint32_t events_published;
    uint32_t events_dispatched;
    uint32_t events_dropped;

    /* Rule engine */
    uint32_t re_loops;
    uint32_t re_rule_evals;
    uint32_t re_rules_fired;
    uint32_t re_errors;

    /* Logging */
    uint32_t log_written;
    uint32_t log_dropped;
} DiagSnapshot_t;

void Diag_Init(void);
void Diag_Collect(DiagSnapshot_t *out);
void Diag_Print(void);

/** Periodic task: collect, publish EVT_DIAG_SNAPSHOT; raise
 *  EVT_DIAG_HEALTH_WARNING if drop/overrun counters increased. */
void Diag_Task(void *arg);

#endif /* DIAG_MGR_H */
