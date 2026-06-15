/**
 * @file EventConfigQueue.h
 * @brief Events published by QueueManager
 *
 * @EVENT_MODULE: Queue
 * @BASE_ID: 0x0800
 */

#ifndef EVENT_CONFIG_QUEUE_H
#define EVENT_CONFIG_QUEUE_H

#include <stdint.h>

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_QUEUE_OVERFLOW       0x0800U
#define EVT_QUEUE_HIGH_WATER     0x0801U
#define EVT_QUEUE_EMPTY          0x0802U

typedef struct {
    uint8_t  queue_id;
    uint16_t depth;
    uint16_t count;          /**< occupancy at time of event              */
    uint32_t total_drops;
    uint32_t timestamp_ms;
} QueuePayload_t;

#endif /* EVENT_CONFIG_QUEUE_H */
