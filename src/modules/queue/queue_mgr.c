/**
 * @file queue_mgr.c
 * @brief Queue Manager implementation
 */

#include "queue_mgr.h"
#include "EventConfigQueue.h"
#include <string.h>
#include <stdio.h>

#ifndef QUEUE_MGR_MAX
#define QUEUE_MGR_MAX 16U
#endif

static struct {
    QueueMgrEntry_t *entries[QUEUE_MGR_MAX];
    uint8_t          count;
} s_qm;

void QueueMgr_Init(void) { (void)memset(&s_qm, 0, sizeof(s_qm)); }

bool QueueMgr_Register(QueueMgrEntry_t *entry)
{
    if (!entry || s_qm.count >= QUEUE_MGR_MAX) return false;
    OSAL_Queue_Init(entry->queue);
    s_qm.entries[s_qm.count++] = entry;
    return true;
}

QueueMgrEntry_t *QueueMgr_FindById(uint8_t id)
{
    uint8_t i;
    for (i = 0U; i < s_qm.count; i++)
        if (s_qm.entries[i]->id == id) return s_qm.entries[i];
    return NULL;
}

QueueMgrEntry_t *QueueMgr_FindByName(const char *name)
{
    uint8_t i;
    if (!name) return NULL;
    for (i = 0U; i < s_qm.count; i++)
        if (strcmp(s_qm.entries[i]->name, name) == 0) return s_qm.entries[i];
    return NULL;
}

bool QueueMgr_Send(uint8_t queue_id, const void *item)
{
    QueueMgrEntry_t *e = QueueMgr_FindById(queue_id);
    if (!e || !item) return false;

    if (OSAL_Queue_Send(e->queue, item, OSAL_NO_WAIT) != OSAL_OK) {
        e->drops++;
        QueuePayload_t p = { e->id, e->depth, OSAL_Queue_Count(e->queue),
                             e->drops, Scheduler_GetTick() };
        EVENT_PUBLISH(EVT_QUEUE_OVERFLOW, &p, sizeof(p));
        return false;
    }
    e->sends++;

    uint16_t occ = OSAL_Queue_Count(e->queue);
    if (occ > e->high_water) e->high_water = occ;

    /* High-water event (edge-triggered) */
    if (e->high_water_pct > 0U) {
        uint16_t threshold = (uint16_t)(((uint32_t)e->depth * e->high_water_pct) / 100U);
        if (occ >= threshold && !e->hw_event_sent) {
            e->hw_event_sent = true;
            QueuePayload_t p = { e->id, e->depth, occ, e->drops, Scheduler_GetTick() };
            EVENT_PUBLISH(EVT_QUEUE_HIGH_WATER, &p, sizeof(p));
        } else if (occ < threshold) {
            e->hw_event_sent = false;
        }
    }
    return true;
}

bool QueueMgr_Recv(uint8_t queue_id, void *item)
{
    QueueMgrEntry_t *e = QueueMgr_FindById(queue_id);
    if (!e || !item) return false;
    if (OSAL_Queue_Recv(e->queue, item, OSAL_NO_WAIT) != OSAL_OK) return false;
    e->receives++;
    return true;
}

uint16_t QueueMgr_Count(uint8_t queue_id)
{
    QueueMgrEntry_t *e = QueueMgr_FindById(queue_id);
    return e ? OSAL_Queue_Count(e->queue) : 0U;
}

void QueueMgr_PrintStats(void)
{
    uint8_t i;
    printf("=== QueueManager stats ===\n");
    for (i = 0U; i < s_qm.count; i++) {
        QueueMgrEntry_t *e = s_qm.entries[i];
        printf("  [%u] %-16s depth=%u occ=%u hw=%u sends=%lu recvs=%lu drops=%lu\n",
               e->id, e->name, e->depth, OSAL_Queue_Count(e->queue), e->high_water,
               (unsigned long)e->sends, (unsigned long)e->receives,
               (unsigned long)e->drops);
    }
}
