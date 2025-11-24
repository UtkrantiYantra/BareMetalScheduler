#ifndef PLATFORM_AVR_H
#define PLATFORM_AVR_H
#include <stdint.h>
#include <avr/interrupt.h>
static inline void CRIT_ENTER(void){ cli(); }
static inline void CRIT_EXIT(void){ sei(); }
static inline void platform_idle(void){ /* use sleep_cpu() if <avr/sleep.h> */ }
static inline uint32_t platform_get_tick(void){ return 0; }
#endif
