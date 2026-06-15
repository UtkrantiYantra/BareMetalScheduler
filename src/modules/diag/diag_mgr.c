/**
 * @file diag_mgr.c
 * @brief Diagnostics subsystem implementation
 */

#include "diag_mgr.h"
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"
#include "EventConfigDiagnostics.h"
#include "re_engine.h"
#include "logging_mgr.h"
#include <string.h>
#include <stdio.h>

static DiagSnapshot_t s_prev;

void Diag_Init(void) { (void)memset(&s_prev, 0, sizeof(s_prev)); }

void Diag_Collect(DiagSnapshot_t *out)
{
    Sched_Diag_t   sd;
    EventBusStats_t eb;
    RE_Diagnostics_t rd;
    LogMgrStats_t   ls;

    if (!out) return;
    (void)memset(out, 0, sizeof(*out));

    out->uptime_ms = Scheduler_GetTick();

    Scheduler_GetDiag(&sd);
    out->sched_ticks      = sd.tick_count;
    out->sched_dispatches = sd.task_dispatches;
    out->sched_overruns   = sd.total_overruns;

    EventBus_GetStats(&eb);
    out->events_published  = eb.published;
    out->events_dispatched = eb.dispatched;
    out->events_dropped    = eb.dropped;

    (void)RE_GetDiagnostics(&rd);
    out->re_loops      = rd.loop_count;
    out->re_rule_evals = rd.rule_eval_count;
    out->re_rules_fired= rd.rule_fire_count;
    out->re_errors     = rd.sensor_read_err + rd.output_write_err;

    LogMgr_GetStats(&ls);
    out->log_written = ls.total_written;
    out->log_dropped = ls.total_dropped;
}

void Diag_Print(void)
{
    DiagSnapshot_t d;
    Diag_Collect(&d);
    printf("=== Diagnostics snapshot (t=%lu ms) ===\n", (unsigned long)d.uptime_ms);
    printf("  Scheduler : ticks=%lu dispatches=%lu overruns=%lu\n",
           (unsigned long)d.sched_ticks, (unsigned long)d.sched_dispatches,
           (unsigned long)d.sched_overruns);
    printf("  EventBus  : pub=%lu disp=%lu dropped=%lu\n",
           (unsigned long)d.events_published, (unsigned long)d.events_dispatched,
           (unsigned long)d.events_dropped);
    printf("  RuleEngine: loops=%lu evals=%lu fired=%lu errs=%lu\n",
           (unsigned long)d.re_loops, (unsigned long)d.re_rule_evals,
           (unsigned long)d.re_rules_fired, (unsigned long)d.re_errors);
    printf("  Logging   : written=%lu dropped=%lu\n",
           (unsigned long)d.log_written, (unsigned long)d.log_dropped);
}

void Diag_Task(void *arg)
{
    (void)arg;
    DiagSnapshot_t d;
    Diag_Collect(&d);

    DiagSnapshotPayload_t p = {
        d.uptime_ms, d.sched_dispatches,
        d.events_published, d.events_dropped,
        d.re_rule_evals, d.log_written
    };
    EVENT_PUBLISH(EVT_DIAG_SNAPSHOT, &p, sizeof(p));

    /* Health warning when drop/overrun counters advance */
    if (d.events_dropped  > s_prev.events_dropped  ||
        d.sched_overruns  > s_prev.sched_overruns  ||
        d.log_dropped     > s_prev.log_dropped     ||
        d.re_errors       > s_prev.re_errors) {
        EVENT_PUBLISH(EVT_DIAG_HEALTH_WARNING, &p, sizeof(p));
    }
    s_prev = d;
}
