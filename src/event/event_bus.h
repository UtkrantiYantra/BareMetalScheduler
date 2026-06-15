/**
 * @file event_bus.h
 * @brief Publish / Subscribe event bus
 *
 * Design
 * ------
 *  - Events are identified by a numeric EventID (uint16_t), drawn from EventList.h
 *    which is auto-generated at build time by merging all EventConfig*.h files.
 *  - Each event carries a fixed-size payload (up to EVENT_MAX_PAYLOAD bytes).
 *  - Subscribers register a callback and optional filter mask.
 *  - EventBus_Publish() is safe to call from both task and ISR context.
 *  - Delivery is deferred: EventBus_Dispatch() drains the internal ring queue
 *    and calls subscriber callbacks. Call from the scheduler or a dedicated task.
 *
 * Macros
 * ------
 *  EVENT_SUBSCRIBE(event_id, callback, arg)
 *  EVENT_PUBLISH(event_id, payload_ptr, payload_len)
 *
 * Example
 * -------
 *  // In AlarmEventManager.c
 *  EVENT_SUBSCRIBE(EVT_TEMP_ALARM_ON, AlarmMgr_OnTempAlarm, NULL);
 *
 *  // In AnalogSensorManager.c
 *  TempAlarmPayload_t p = { .sensor_id = 1, .value_x100 = 5500 };
 *  EVENT_PUBLISH(EVT_TEMP_ALARM_ON, &p, sizeof(p));
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "EventList.h"   /* auto-generated: all EVT_* constants */

/* =========================================================================
 * Configuration (override via compiler -D flags)
 * ========================================================================= */
#ifndef EVENT_MAX_SUBSCRIBERS
#define EVENT_MAX_SUBSCRIBERS  64U
#endif

#ifndef EVENT_QUEUE_DEPTH
#define EVENT_QUEUE_DEPTH      32U
#endif

#ifndef EVENT_MAX_PAYLOAD
#define EVENT_MAX_PAYLOAD      32U  /* bytes per event payload */
#endif

/* =========================================================================
 * Event envelope (what lives in the ring queue)
 * ========================================================================= */
typedef struct {
    EventID_t  id;
    uint8_t    payload[EVENT_MAX_PAYLOAD];
    uint8_t    payload_len;
} EventEnvelope_t;

/* =========================================================================
 * Subscriber callback
 * ========================================================================= */
typedef void (*EventCallback_t)(EventID_t id, const void *payload,
                                uint8_t payload_len, void *arg);

/* =========================================================================
 * Subscription record
 * ========================================================================= */
typedef struct {
    EventID_t       event_id;
    EventCallback_t cb;
    void           *arg;
    bool            active;
} EventSubscription_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** @brief Initialise event bus. Call once before any publish/subscribe. */
void EventBus_Init(void);

/**
 * @brief Subscribe to an event.
 * @param event_id  Event to listen for (EVT_* constant from EventList.h).
 * @param cb        Callback invoked when the event is dispatched.
 * @param arg       Opaque user pointer passed to cb.
 * @return Subscription handle (index), or -1 on failure.
 */
int16_t EventBus_Subscribe(EventID_t event_id, EventCallback_t cb, void *arg);

/**
 * @brief Unsubscribe using the handle returned by EventBus_Subscribe.
 */
void EventBus_Unsubscribe(int16_t handle);

/**
 * @brief Publish an event (ISR-safe). Copies payload into the ring queue.
 * @param event_id    EVT_* constant.
 * @param payload     Pointer to payload data (may be NULL if len == 0).
 * @param payload_len Number of bytes to copy (max EVENT_MAX_PAYLOAD).
 * @return true if queued, false if ring is full.
 */
bool EventBus_Publish(EventID_t event_id, const void *payload, uint8_t payload_len);

/**
 * @brief Drain the event queue and invoke subscriber callbacks.
 * Call this from Scheduler_Run() / a high-priority task.
 * @param max_events Maximum events to dispatch per call (0 = drain all).
 * @return Number of events dispatched.
 */
uint16_t EventBus_Dispatch(uint16_t max_events);

/** @brief Number of events currently waiting in the queue. */
uint16_t EventBus_Pending(void);

/** @brief Flush all pending events without dispatching them. */
void EventBus_Flush(void);

/* Bus statistics for the diagnostics subsystem */
typedef struct {
    uint32_t published;
    uint32_t dispatched;
    uint32_t dropped;
    uint16_t subscriber_count;
} EventBusStats_t;

void EventBus_GetStats(EventBusStats_t *out);

/* =========================================================================
 * Advanced payloads — pool-backed large/zero-copy buffers
 * =========================================================================
 * For payloads larger than EVENT_MAX_PAYLOAD, allocate a block from the
 * bus pool, fill it, and publish by reference.  The envelope carries only
 * the pointer; the LAST subscriber dispatched frees the block automatically.
 *
 *   void *buf = EventBus_PayloadAlloc();
 *   memcpy(buf, big_data, n);
 *   EventBus_PublishRef(EVT_BULK_DATA, buf, n);
 */
#ifndef EVENT_POOL_BLOCK_SIZE
#define EVENT_POOL_BLOCK_SIZE  128U
#endif
#ifndef EVENT_POOL_BLOCKS
#define EVENT_POOL_BLOCKS      16U
#endif

/** Allocate one EVENT_POOL_BLOCK_SIZE-byte block (NULL if exhausted). */
void *EventBus_PayloadAlloc(void);

/** Manually release a pool block (normally automatic after dispatch). */
void  EventBus_PayloadFree(void *blk);

/** Publish by reference: subscribers receive the pointer in payload.
 *  The block is freed by the bus after the last subscriber callback. */
bool  EventBus_PublishRef(EventID_t event_id, void *pool_block, uint16_t len);

/* =========================================================================
 * Convenience macros
 * ========================================================================= */

/** Subscribe a callback to an event. */
#define EVENT_SUBSCRIBE(event_id_, cb_, arg_) \
    EventBus_Subscribe((event_id_), (cb_), (arg_))

/** Publish an event with typed payload. payload_ptr may be NULL for no-data events. */
#define EVENT_PUBLISH(event_id_, payload_ptr_, len_) \
    EventBus_Publish((event_id_), (payload_ptr_), (uint8_t)(len_))

/** Publish a zero-payload event (signal only). */
#define EVENT_SIGNAL(event_id_) \
    EventBus_Publish((event_id_), NULL, 0U)

#endif /* EVENT_BUS_H */
