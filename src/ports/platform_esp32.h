#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H
#include <stdint.h>
/* ESP32 integration: use portENTER_CRITICAL/portEXIT_CRITICAL if using FreeRTOS,
   or __disable_irq/enable if bare-metal */
static inline void CRIT_ENTER(void){ /* implement */ }
static inline void CRIT_EXIT(void){ /* implement */ }
static inline void platform_idle(void){ /* esp-idle */ }
static inline uint32_t platform_get_tick(void){ return 0; }
#endif
