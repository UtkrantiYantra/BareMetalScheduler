/**
 * @file event_bus.c
 * @brief Publish / Subscribe event bus implementation
 */

#include "event_bus.h"
#include <string.h>

/* Pool storage for by-reference payloads */
static uint8_t  s_pool[EVENT_POOL_BLOCKS][EVENT_POOL_BLOCK_SIZE];
static uint8_t  s_pool_used[EVENT_POOL_BLOCKS];

/* Envelope flag: payload field holds {ptr,len} instead of inline bytes */
#define ENV_FLAG_REF  0x80U

/* =========================================================================
 * Internal state
 * ========================================================================= */
static struct {
    /* Subscriber table */
    EventSubscription_t subs[EVENT_MAX_SUBSCRIBERS];
    uint16_t            sub_count;

    /* Ring queue (lock-free SPSC for ISR publish + task dispatch) */
    EventEnvelope_t  ring[EVENT_QUEUE_DEPTH];
    volatile uint16_t head;   /* write index (publisher advances) */
    volatile uint16_t tail;   /* read  index (dispatcher advances) */

    /* Diagnostics */
    uint32_t published;
    uint32_t dispatched;
    uint32_t dropped;         /* published when ring was full */
    bool     initialized;
} s_bus;

/* =========================================================================
 * Init
 * ========================================================================= */
void EventBus_Init(void)
{
    (void)memset(&s_bus, 0, sizeof(s_bus));
    s_bus.initialized = true;
}

/* =========================================================================
 * Subscribe
 * ========================================================================= */
int16_t EventBus_Subscribe(EventID_t event_id, EventCallback_t cb, void *arg)
{
    uint16_t i;
    if (!cb) return -1;

    /* Re-use a freed slot first */
    for (i = 0U; i < s_bus.sub_count; i++) {
        if (!s_bus.subs[i].active) {
            s_bus.subs[i].event_id = event_id;
            s_bus.subs[i].cb       = cb;
            s_bus.subs[i].arg      = arg;
            s_bus.subs[i].active   = true;
            return (int16_t)i;
        }
    }

    if (s_bus.sub_count >= EVENT_MAX_SUBSCRIBERS) return -1;

    i = s_bus.sub_count++;
    s_bus.subs[i].event_id = event_id;
    s_bus.subs[i].cb       = cb;
    s_bus.subs[i].arg      = arg;
    s_bus.subs[i].active   = true;
    return (int16_t)i;
}

/* =========================================================================
 * Unsubscribe
 * ========================================================================= */
void EventBus_Unsubscribe(int16_t handle)
{
    if (handle >= 0 && (uint16_t)handle < s_bus.sub_count) {
        s_bus.subs[handle].active = false;
    }
}

/* =========================================================================
 * Publish (ISR-safe: no locks, SPSC ring)
 * ========================================================================= */
bool EventBus_Publish(EventID_t event_id, const void *payload, uint8_t payload_len)
{
    uint16_t next_head;

    if (!s_bus.initialized) return false;

    next_head = (uint16_t)((s_bus.head + 1U) % EVENT_QUEUE_DEPTH);
    if (next_head == s_bus.tail) {
        /* Ring full */
        s_bus.dropped++;
        return false;
    }

    EventEnvelope_t *env = &s_bus.ring[s_bus.head];
    env->id = event_id;

    if (payload && payload_len > 0U) {
        uint8_t copy_len = (payload_len > EVENT_MAX_PAYLOAD) ? EVENT_MAX_PAYLOAD : payload_len;
        (void)memcpy(env->payload, payload, copy_len);
        env->payload_len = copy_len;
    } else {
        env->payload_len = 0U;
    }

    /* Barrier: write payload before advancing head */
    s_bus.head = next_head;
    s_bus.published++;
    return true;
}

/* =========================================================================
 * Dispatch — drain ring, call subscribers
 * ========================================================================= */
uint16_t EventBus_Dispatch(uint16_t max_events)
{
    uint16_t dispatched = 0U;
    uint16_t i;

    if (max_events == 0U) max_events = EVENT_QUEUE_DEPTH;

    while ((s_bus.tail != s_bus.head) && (dispatched < max_events)) {
        const EventEnvelope_t *env = &s_bus.ring[s_bus.tail];
        EventID_t id = env->id;

        /* Deliver to all matching subscribers */
        for (i = 0U; i < s_bus.sub_count; i++) {
            if (s_bus.subs[i].active && s_bus.subs[i].event_id == id) {
                s_bus.subs[i].cb(id, env->payload, env->payload_len, s_bus.subs[i].arg);
            }
        }

        /* Auto-free pool block if this envelope carried a by-reference
         * payload (a {ptr,len} struct whose ptr lies inside s_pool). */
        if (env->payload_len == (uint8_t)sizeof(struct { void *p; uint16_t l; })) {
            void *p;
            (void)memcpy(&p, env->payload, sizeof(p));
            if ((uint8_t*)p >= &s_pool[0][0] &&
                (uint8_t*)p <  &s_pool[0][0] + sizeof(s_pool)) {
                EventBus_PayloadFree(p);
            }
        }

        s_bus.tail = (uint16_t)((s_bus.tail + 1U) % EVENT_QUEUE_DEPTH);
        dispatched++;
        s_bus.dispatched++;
    }

    return dispatched;
}

/* =========================================================================
 * Helpers
 * ========================================================================= */
uint16_t EventBus_Pending(void)
{
    return (uint16_t)((s_bus.head - s_bus.tail + EVENT_QUEUE_DEPTH) % EVENT_QUEUE_DEPTH);
}

void EventBus_Flush(void)
{
    s_bus.tail = s_bus.head;
}

void *EventBus_PayloadAlloc(void)
{
    uint8_t i;
    for (i = 0U; i < EVENT_POOL_BLOCKS; i++) {
        if (!s_pool_used[i]) { s_pool_used[i] = 1U; return s_pool[i]; }
    }
    return NULL;
}

void EventBus_PayloadFree(void *blk)
{
    uint8_t i;
    for (i = 0U; i < EVENT_POOL_BLOCKS; i++) {
        if ((void*)s_pool[i] == blk) { s_pool_used[i] = 0U; return; }
    }
}

bool EventBus_PublishRef(EventID_t event_id, void *pool_block, uint16_t len)
{
    struct { void *ptr; uint16_t len; } ref;
    bool ok;
    if (!pool_block) return false;
    ref.ptr = pool_block;
    ref.len = len;
    /* Re-use the inline path to carry the {ptr,len} reference;
     * dispatcher frees after the last subscriber via the REF flag bit
     * encoded in payload_len's top bit being unavailable — instead we
     * detect by pointer range. */
    ok = EventBus_Publish(event_id, &ref, (uint8_t)sizeof(ref));
    if (!ok) EventBus_PayloadFree(pool_block);
    return ok;
}

void EventBus_GetStats(EventBusStats_t *out)
{
    if (!out) return;
    out->published        = s_bus.published;
    out->dispatched       = s_bus.dispatched;
    out->dropped          = s_bus.dropped;
    out->subscriber_count = s_bus.sub_count;
}
