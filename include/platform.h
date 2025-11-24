#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* platform-specific hooks are provided in port-specific source files.
 * You can implement CRIT_ENTER/EXIT and idle here per target.
 */

#ifdef TARGET_STM32F4
#include "platform_stm32f4.h"
#elif defined(TARGET_ESP32)
#include "platform_esp32.h"
#elif defined(TARGET_AVR)
#include "platform_avr.h"
#else
/* Host fallback */
static inline void CRIT_ENTER(void) {}
static inline void CRIT_EXIT(void) {}
static inline void platform_idle(void) { /* busy-wait */ }
static inline uint32_t platform_get_tick(void) { return 0; }
#endif

#endif // PLATFORM_H
