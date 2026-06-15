/**
 * @file EventConfigDiagnostics.h
 * @brief Events published by DiagnosticsModule
 *
 * @EVENT_MODULE: Diagnostics
 * @BASE_ID: 0x0A00
 */

#ifndef EVENT_CONFIG_DIAGNOSTICS_H
#define EVENT_CONFIG_DIAGNOSTICS_H

#include <stdint.h>

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_DIAG_SNAPSHOT        0x0A00U
#define EVT_DIAG_HEALTH_WARNING  0x0A01U

typedef struct {
    uint32_t uptime_ms;
    uint32_t sched_dispatches;
    uint32_t events_published;
    uint32_t events_dropped;
    uint32_t rule_evals;
    uint32_t log_entries;
} DiagSnapshotPayload_t;

#endif /* EVENT_CONFIG_DIAGNOSTICS_H */
