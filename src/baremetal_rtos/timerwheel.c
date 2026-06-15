#include "timerwheel.h"
#include "event_registry.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

typedef struct tnode {
    struct tnode *next;
    uint32_t expiry;
    uint32_t period;
    uint8_t task;
    event_id_t event;
    uint32_t handle;
} tnode_t;

static tnode_t **wheel = NULL;
static uint32_t wheel_size = 0;
static uint32_t wheel_mask = 0;
static uint32_t curr = 0;
static uint32_t next_handle = 1;

void timerwheel_init(uint32_t tick_ms, uint32_t size)
{
    (void)tick_ms;
    if(wheel) return;
    wheel_size = size;
    wheel_mask = size - 1;
    wheel = (tnode_t**)malloc(sizeof(tnode_t*)*size);
    memset(wheel,0,sizeof(tnode_t*)*size);
    curr = 0;
    next_handle = 1;
}

static void place(tnode_t *n){
    uint32_t slot = n->expiry & wheel_mask;
    CRIT_ENTER();
    n->next = wheel[slot];
    wheel[slot] = n;
    CRIT_EXIT();
}

uint32_t timerwheel_start(uint8_t target_task, event_id_t ev, uint32_t ticks, uint32_t periodic_ticks)
{
    if(!wheel) return 0;
    tnode_t *n = (tnode_t*)malloc(sizeof(tnode_t));
    if(!n) return 0;
    n->expiry = curr + ticks;
    n->period = periodic_ticks;
    n->task = target_task;
    n->event = ev;
    n->handle = next_handle++;
    place(n);
    return n->handle;
}

void timerwheel_cancel(uint32_t handle)
{
    if(!wheel) return;
    CRIT_ENTER();
    for(uint32_t s=0;s<wheel_size;s++){
        tnode_t **pp = &wheel[s];
        while(*pp){
            if((*pp)->handle == handle){
                tnode_t *rm = *pp;
                *pp = rm->next;
                free(rm);
                CRIT_EXIT();
                return;
            }
            pp = &(*pp)->next;
        }
    }
    CRIT_EXIT();
}

void timerwheel_tick(void)
{
    if(!wheel) return;
    uint32_t slot = curr & wheel_mask;
    CRIT_ENTER();
    tnode_t *head = wheel[slot];
    wheel[slot] = NULL;
    CRIT_EXIT();

    tnode_t *rem = NULL;
    while(head){
        tnode_t *nx = head->next;
        if(head->expiry <= curr){
            /* fire */
            event_publish(head->event, NULL, 0, EVT_FLAG_COPY);
            if(head->period){
                head->expiry = curr + head->period;
                head->next = rem; rem = head;
            } else {
                free(head);
            }
        } else {
            head->next = rem; rem = head;
        }
        head = nx;
    }

    /* reinsert */
    CRIT_ENTER();
    tnode_t *r = rem;
    while(r){
        tnode_t *n = r; r = r->next;
        uint32_t s = n->expiry & wheel_mask;
        n->next = wheel[s]; wheel[s] = n;
    }
    CRIT_EXIT();

    curr++;
}
