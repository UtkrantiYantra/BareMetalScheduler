#ifndef PLATFORM_HOST_H
#define PLATFORM_HOST_H
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
static inline void CRIT_ENTER(void){ /* for host, not necessary */ }
static inline void CRIT_EXIT(void){ /* no-op */ }
static inline void platform_idle(void){ usleep(1000); }
static inline uint32_t platform_get_tick(void){ return 0; }
#endif
