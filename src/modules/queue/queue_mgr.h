/**
 * @file queue_mgr.h
 * @brief Queue Manager — named message queues over OSAL with diagnostics
 *
 * Central registry of message queues so modules can exchange data by
 * queue ID/name instead of sharing raw pointers.  Built on OSAL_Queue_t,
 * so it transparently works on bare-metal / FreeRTOS / ThreadX.
 *
 * Features:
 *   - QUEUE_MGR_DEF macro: static storage allocation
 *   - Per-queue statistics (sends, drops, high-water mark)
 *   - EVT_QUEUE_OVERFLOW / EVT_QUEUE_HIGH_WATER events
 *   - Lookup by ID or name
 */

#ifndef QUEUE_MGR_H
#define QUEUE_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "osal.h"
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

typedef struct {
    uint8_t       id;
    const char   *name;
    OSAL_Queue_t *queue;          /**< Underlying OSAL queue                  */
    uint16_t      item_size;
    uint16_t      depth;
    uint16_t      high_water_pct; /**< Publish EVT_QUEUE_HIGH_WATER above this % (0=off) */

    /* Stats */
    uint32_t      sends;
    uint32_t      receives;
    uint32_t      drops;
    uint16_t      high_water;     /**< Max occupancy ever observed            */
    bool          hw_event_sent;  /**< Suppress repeated high-water events    */
} QueueMgrEntry_t;

/* =========================================================================
 * QUEUE_MGR_DEF — declare queue storage + registry entry in one macro
 *
 *   QUEUE_MGR_DEF(g_sensor_q, 1, "sensor_readings", sizeof(Reading_t), 16, 75);
 *   ...
 *   QueueMgr_Register(&g_sensor_q);
 * ========================================================================= */
#define QUEUE_MGR_DEF(var, id_, name_, item_size_, depth_, hw_pct_)   \
    OSAL_QUEUE_DEF(var##_osal, (item_size_), (depth_));                \
    static QueueMgrEntry_t var = {                                     \
        (id_), (name_), &var##_osal, (item_size_), (depth_),          \
        (hw_pct_), 0U, 0U, 0U, 0U, false                               \
    }

/* API */
void              QueueMgr_Init(void);
bool              QueueMgr_Register(QueueMgrEntry_t *entry);
QueueMgrEntry_t  *QueueMgr_FindById(uint8_t id);
QueueMgrEntry_t  *QueueMgr_FindByName(const char *name);

/** Send: returns false + EVT_QUEUE_OVERFLOW on full queue. */
bool QueueMgr_Send(uint8_t queue_id, const void *item);

/** Receive: returns false if empty. */
bool QueueMgr_Recv(uint8_t queue_id, void *item);

/** Occupancy. */
uint16_t QueueMgr_Count(uint8_t queue_id);

/** Print stats for all queues. */
void QueueMgr_PrintStats(void);

#endif /* QUEUE_MGR_H */
