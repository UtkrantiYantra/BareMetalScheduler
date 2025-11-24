#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

static log_stats_t stats = {0};

void log_init(void)
{
    stats.events_published = 0;
    stats.events_dropped = 0;
}

void log_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

const log_stats_t* log_get_stats(void) { return &stats; }

/* Simple profiling hooks using host clock via clock() */
#include <time.h>
typedef struct {
    const char *name;
    clock_t start;
    clock_t total;
    uint32_t count;
} prof_t;

#define MAX_PROF 32
static prof_t profiles[MAX_PROF];

void profile_mark_start(const char *name)
{
    for(int i=0;i<MAX_PROF;i++){
        if(profiles[i].name == NULL){ profiles[i].name = name; profiles[i].start = clock(); return; }
        if(strcmp(profiles[i].name, name)==0){ profiles[i].start = clock(); return; }
    }
}

void profile_mark_end(const char *name)
{
    for(int i=0;i<MAX_PROF;i++){
        if(profiles[i].name && strcmp(profiles[i].name, name)==0){
            clock_t d = clock() - profiles[i].start;
            profiles[i].total += d;
            profiles[i].count++;
            return;
        }
    }
}
