#include "event_registry.h"
#include "task.h"
#include "logging.h"
#include "platform.h"
#include <string.h>

#ifndef MAX_GLOBAL_EVENTS
#define MAX_GLOBAL_EVENTS 128
#endif

extern void* task_payload_alloc(size_t size);
extern log_stats_t* log_get_stats_mut(void);

typedef struct {
    event_id_t id;
    uint32_t subs_mask; /* supports up to 32 tasks; extend if needed */
} entry_t;

static entry_t registry[MAX_GLOBAL_EVENTS];

void event_registry_init(void)
{
    for(int i=0;i<MAX_GLOBAL_EVENTS;i++){
        registry[i].id = (event_id_t)i;
        registry[i].subs_mask = 0;
    }
    log_init();
}

void event_subscribe(uint8_t task_idx, event_id_t id)
{
    if(id >= MAX_GLOBAL_EVENTS) return;
    CRIT_ENTER();
    registry[id].subs_mask |= (1u<<task_idx);
    CRIT_EXIT();
}

void event_unsubscribe(uint8_t task_idx, event_id_t id)
{
    if(id >= MAX_GLOBAL_EVENTS) return;
    CRIT_ENTER();
    registry[id].subs_mask &= ~(1u<<task_idx);
    CRIT_EXIT();
}

void event_publish(event_id_t id, void *payload, size_t size, uint32_t flags)
{
    if(id >= MAX_GLOBAL_EVENTS) return;
    uint32_t subs;
    CRIT_ENTER();
    subs = registry[id].subs_mask;
    CRIT_EXIT();
    if(subs == 0){
        log_get_stats_mut()->events_dropped++;
        return;
    }

    /* For each subscriber, post event.
     * COPY semantics: allocate block per subscriber.
     * ZERO_COPY: payload is transferred to first subscriber only.
     */
    int zero_copy_sent = 0;
    for(uint8_t t=0; t<32; ++t){
        if(subs & (1u<<t)){
            if(flags & EVT_FLAG_ZERO_COPY){
                void *pl = NULL;
                if(!zero_copy_sent){ pl = payload; zero_copy_sent = 1; }
                event_t e = { .id = id, .payload = pl, .size = size };
                task_enqueue_isr(t, &e);
            } else {
                /* copy mode */
                void *blk = task_payload_alloc(size);
                if(blk && payload && size) memcpy(blk, payload, size);
                event_t e = { .id = id, .payload = blk, .size = size };
                task_enqueue_isr(t, &e);
            }
            log_get_stats_mut()->events_published++;
        }
    }
}
