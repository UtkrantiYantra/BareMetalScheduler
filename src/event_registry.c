#include "event_registry.h"
#include "task.h"
#include "logging.h"
#include "platform.h"
#include <string.h>

#ifndef MAX_GLOBAL_EVENTS
#define MAX_GLOBAL_EVENTS 128
#endif

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
        log_get_stats()->events_dropped++;
        return;
    }

    /* For each subscriber, post event.
     * COPY semantics: allocate block per subscriber.
     * ZERO_COPY: payload is transferred to first subscriber and must be documented.
     */
    for(uint8_t t=0; t<32; ++t){
        if(subs & (1u<<t)){
            if(flags & EVT_FLAG_ZERO_COPY){
                /* transfer ownership to the first subscriber only - others will get NULL payload
                   (application-level policy). Here we post payload to all but mark that only
                   the first receives pointer. */
                static int sent_once = 0;
                void *pl = NULL;
                if(!sent_once){ pl = payload; sent_once = 1; }
                task_enqueue_isr(t, &(event_t){ .id = id, .payload = pl, .size = size });
            } else {
                /* copy mode */
                void *blk = task_payload_alloc(size);
                if(blk && payload && size) memcpy(blk, payload, size);
                task_enqueue_isr(t, &(event_t){ .id = id, .payload = blk, .size = size });
            }
            log_get_stats()->events_published++;
        }
    }
    /* If COPY mode, nothing else. If ZERO_COPY, caller must not free payload - ownership moved */
}
