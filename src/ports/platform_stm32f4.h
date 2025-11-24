#ifndef PLATFORM_STM32F4_H
#define PLATFORM_STM32F4_H
#include <stdint.h>
/* Provide implementations for CRIT_ENTER/EXIT, platform_idle, and tick */
static inline void CRIT_ENTER(void){ __asm volatile("cpsid i":::"memory"); }
static inline void CRIT_EXIT(void){ __asm volatile("cpsie i":::"memory"); }
static inline void platform_idle(void){ __asm volatile("wfi"); }
static inline uint32_t platform_get_tick(void){ /* integrate with HAL */ return 0; }
#endif
