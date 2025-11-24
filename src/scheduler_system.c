#include "event_registry.h"
#include "timerwheel.h"
#include "task.h"
#include "platform.h"

void system_init(void){
    event_registry_init();
    timerwheel_init(1, 256);
}

void system_start(void){
    task_worker_loop();
}

void system_tick(void){
    timerwheel_tick();
}
