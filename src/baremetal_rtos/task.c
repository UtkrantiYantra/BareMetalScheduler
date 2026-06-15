#include "task.h"
#include "platform.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int used;
    uint8_t priority;
    task_handler_t handler;
    event_t queue[TASK_QUEUE_DEPTH];
    uint8_t head, tail;
} tcb_t;

static tcb_t tasks[MAX_TASKS];
static int task_cnt = 0;

/* helper to allocate payload - implemented in mempool module via exported registry */
extern void* task_payload_alloc(size_t size);

int task_create(task_handler_t handler, uint8_t priority)
{
    CRIT_ENTER();
    if(task_cnt >= MAX_TASKS){ CRIT_EXIT(); return -1; }
    int idx = task_cnt++;
    tasks[idx].used = 1;
    tasks[idx].priority = priority;
    tasks[idx].handler = handler;
    tasks[idx].head = tasks[idx].tail = 0;
    CRIT_EXIT();
    return idx;
}

void task_enqueue_isr(int task_idx, const event_t *evt)
{
    if(task_idx < 0 || task_idx >= task_cnt) return;
    CRIT_ENTER();
    tcb_t *t = &tasks[task_idx];
    uint8_t next = (t->tail+1) % TASK_QUEUE_DEPTH;
    if(next == t->head){
        /* drop */
        CRIT_EXIT();
        return;
    }
    t->queue[t->tail] = *evt;
    t->tail = next;
    CRIT_EXIT();
}

void task_enqueue(int task_idx, const event_t *evt)
{
    task_enqueue_isr(task_idx, evt);
}

void task_payload_free(void *ptr)
{
    /* forward to mempool free via platform-agnostic API */
    extern void task_payload_free_impl(void *);
    task_payload_free_impl(ptr);
}

void task_worker_loop(void)
{
    while(1){
        int best = -1;
        uint8_t best_prio = 255;
        CRIT_ENTER();
        for(int i=0;i<task_cnt;i++){
            if(!tasks[i].used) continue;
            if(tasks[i].head == tasks[i].tail) continue;
            if(tasks[i].priority < best_prio){ best_prio = tasks[i].priority; best = i; }
        }
        CRIT_EXIT();
        if(best < 0){ platform_idle(); continue; }

        /* pop one event */
        CRIT_ENTER();
        tcb_t *t = &tasks[best];
        event_t evt = t->queue[t->head];
        t->head = (t->head + 1) % TASK_QUEUE_DEPTH;
        CRIT_EXIT();

        /* profile start */
        profile_mark_start("task"); /* simple name */
        t->handler(&evt);
        profile_mark_end("task");

        /* free payload if present */
        if(evt.payload) task_payload_free(evt.payload);
    }
}
