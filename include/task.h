#ifndef TASK_H
#define TASK_H
#include "event.h"
#include <stdint.h>
#include <stddef.h>

#ifndef MAX_TASKS
#define MAX_TASKS 32
#endif

#ifndef TASK_QUEUE_DEPTH
#define TASK_QUEUE_DEPTH 16
#endif

typedef void (*task_handler_t)(const event_t *evt);

int task_create(task_handler_t handler, uint8_t priority);
void task_enqueue_isr(int task_idx, const event_t *evt);
void task_enqueue(int task_idx, const event_t *evt);
void task_worker_loop(void);
void task_payload_free(void *ptr);

#endif // TASK_H
