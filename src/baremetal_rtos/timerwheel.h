#ifndef TIMERWHEEL_H
#define TIMERWHEEL_H
#include <stdint.h>
#include "event.h"

void timerwheel_init(uint32_t tick_ms, uint32_t wheel_size);
uint32_t timerwheel_start(uint8_t target_task, event_id_t event_id, uint32_t ticks, uint32_t periodic_ticks);
void timerwheel_cancel(uint32_t handle);
void timerwheel_tick(void);

#endif // TIMERWHEEL_H
