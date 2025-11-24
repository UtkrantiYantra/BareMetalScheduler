#ifndef EVENT_REGISTRY_H
#define EVENT_REGISTRY_H
#include "event.h"
#include <stdint.h>

void event_registry_init(void);
void event_subscribe(uint8_t task_idx, event_id_t id);
void event_unsubscribe(uint8_t task_idx, event_id_t id);

/* Flags: copy vs zero-copy */
#define EVT_FLAG_COPY      0x0
#define EVT_FLAG_ZERO_COPY 0x1

/* Safe from ISR */
void event_publish(event_id_t id, void *payload, size_t size, uint32_t flags);

#endif // EVENT_REGISTRY_H
