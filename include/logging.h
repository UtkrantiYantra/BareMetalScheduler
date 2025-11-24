#ifndef LOGGING_H
#define LOGGING_H
#include <stdint.h>

void log_init(void);
void log_printf(const char *fmt, ...);

/* Profiling hooks */
void profile_mark_start(const char *name);
void profile_mark_end(const char *name);

/* Expose simple stats */
typedef struct {
    uint32_t events_published;
    uint32_t events_dropped;
} log_stats_t;

const log_stats_t* log_get_stats(void);

#endif // LOGGING_H
