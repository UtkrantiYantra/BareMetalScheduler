/**
 * @file osal.h
 * @brief OS Abstraction Layer (OSAL)
 *
 * Single header that maps scheduler primitives to either:
 *   - Bare-metal (default): inline implementations using the bare-metal scheduler
 *   - FreeRTOS:  define OSAL_FREERTOS before including
 *   - ThreadX:   define OSAL_THREADX  before including
 *
 * Modules written against this header compile unchanged on all three targets.
 *
 * Primitives provided:
 *   OSAL_Queue    – fixed-depth message queue (send/receive, non-blocking option)
 *   OSAL_Mutex    – mutual exclusion lock
 *   OSAL_Timer    – one-shot or periodic software timer
 *   OSAL_Task     – task / thread handle (for RTOS targets)
 *   OSAL_Tick     – system tick type (milliseconds)
 */

#ifndef OSAL_H
#define OSAL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* =========================================================================
 * Target selection
 * ========================================================================= */
#if defined(OSAL_FREERTOS)
#   include "FreeRTOS.h"
#   include "queue.h"
#   include "semphr.h"
#   include "timers.h"
#elif defined(OSAL_THREADX)
#   include "tx_api.h"
#else
#   define OSAL_BAREMETAL 1
#endif

/* =========================================================================
 * Common tick type
 * ========================================================================= */
typedef uint32_t OSAL_Tick_t;
#define OSAL_TICK_MAX  UINT32_MAX
#define OSAL_NO_WAIT   ((OSAL_Tick_t)0U)
#define OSAL_WAIT_FOREVER ((OSAL_Tick_t)0xFFFFFFFFU)

/* =========================================================================
 * Return codes
 * ========================================================================= */
typedef enum {
    OSAL_OK        = 0,
    OSAL_ERR_FULL  = 1,
    OSAL_ERR_EMPTY = 2,
    OSAL_ERR_TIMEOUT = 3,
    OSAL_ERR_NULL  = 4,
    OSAL_ERR_PARAM = 5,
} OSAL_Status_t;

/* =========================================================================
 * ░░  QUEUE
 * =========================================================================
 * OSAL_QUEUE_DEF(name, item_size, depth)  – allocate static storage
 * OSAL_Queue_Init(&q)
 * OSAL_Queue_Send(&q, &item, timeout_ms)
 * OSAL_Queue_Recv(&q, &item, timeout_ms)
 * OSAL_Queue_Peek(&q, &item)
 * OSAL_Queue_Count(&q)   – items currently in queue
 * ========================================================================= */
#if defined(OSAL_FREERTOS)

typedef struct { QueueHandle_t handle; uint16_t item_size; } OSAL_Queue_t;
#define OSAL_QUEUE_DEF(name, item_size_, depth_) \
    static OSAL_Queue_t name = {NULL, (item_size_)}
#define OSAL_Queue_Init(q) \
    do { (q)->handle = xQueueCreate((depth_), (q)->item_size); } while(0)
static inline OSAL_Status_t OSAL_Queue_Send(OSAL_Queue_t *q, const void *item, OSAL_Tick_t ticks) {
    return (xQueueSend(q->handle, item, ticks) == pdTRUE) ? OSAL_OK : OSAL_ERR_FULL; }
static inline OSAL_Status_t OSAL_Queue_Recv(OSAL_Queue_t *q, void *item, OSAL_Tick_t ticks) {
    return (xQueueReceive(q->handle, item, ticks) == pdTRUE) ? OSAL_OK : OSAL_ERR_EMPTY; }
static inline OSAL_Status_t OSAL_Queue_Peek(OSAL_Queue_t *q, void *item) {
    return (xQueuePeek(q->handle, item, 0) == pdTRUE) ? OSAL_OK : OSAL_ERR_EMPTY; }
static inline uint16_t OSAL_Queue_Count(OSAL_Queue_t *q) {
    return (uint16_t)uxQueueMessagesWaiting(q->handle); }

#elif defined(OSAL_THREADX)

typedef struct { TX_QUEUE handle; } OSAL_Queue_t;
/* ThreadX queue word-aligned items; item_size must be multiple of 4 */
#define OSAL_QUEUE_DEF(name, item_size_, depth_) \
    static ULONG _##name##_buf[(depth_) * ((item_size_) / 4U)]; \
    static OSAL_Queue_t name
#define OSAL_Queue_Init(q) \
    tx_queue_create(&(q)->handle, #q, TX_1_ULONG, _##q##_buf, sizeof(_##q##_buf))
/* Simplified: caller responsible for item_size == 4 */
static inline OSAL_Status_t OSAL_Queue_Send(OSAL_Queue_t *q, const void *item, OSAL_Tick_t ticks) {
    return (tx_queue_send(&q->handle, (void*)item, ticks) == TX_SUCCESS) ? OSAL_OK : OSAL_ERR_FULL; }
static inline OSAL_Status_t OSAL_Queue_Recv(OSAL_Queue_t *q, void *item, OSAL_Tick_t ticks) {
    return (tx_queue_receive(&q->handle, item, ticks) == TX_SUCCESS) ? OSAL_OK : OSAL_ERR_EMPTY; }
static inline OSAL_Status_t OSAL_Queue_Peek(OSAL_Queue_t *q, void *item) {
    return (tx_queue_front_send(&q->handle, item, TX_NO_WAIT) == TX_SUCCESS) ? OSAL_OK : OSAL_ERR_EMPTY; }
static inline uint16_t OSAL_Queue_Count(OSAL_Queue_t *q) {
    ULONG n; tx_queue_info_get(&q->handle,NULL,&n,NULL,NULL,NULL,NULL); return (uint16_t)n; }

#else /* OSAL_BAREMETAL */

/* Lock-free single-producer single-consumer ring buffer */
typedef struct {
    uint8_t  *buf;        /* raw storage (allocated by OSAL_QUEUE_DEF) */
    uint16_t  item_size;
    uint16_t  depth;
    volatile uint16_t head; /* write index */
    volatile uint16_t tail; /* read  index */
} OSAL_Queue_t;

#define OSAL_QUEUE_DEF(name, item_size_, depth_)                     \
    static uint8_t _##name##_buf[(item_size_) * (depth_)];           \
    static OSAL_Queue_t name = {                                      \
        _##name##_buf, (item_size_), (depth_), 0U, 0U }

static inline OSAL_Status_t OSAL_Queue_Init(OSAL_Queue_t *q) {
    q->head = 0U; q->tail = 0U; return OSAL_OK; }

static inline OSAL_Status_t OSAL_Queue_Send(OSAL_Queue_t *q, const void *item, OSAL_Tick_t timeout_ms) {
    (void)timeout_ms;
    uint16_t next = (uint16_t)((q->head + 1U) % q->depth);
    if (next == q->tail) return OSAL_ERR_FULL;
    (void)memcpy(q->buf + (q->head * q->item_size), item, q->item_size);
    q->head = next;
    return OSAL_OK;
}

static inline OSAL_Status_t OSAL_Queue_Recv(OSAL_Queue_t *q, void *item, OSAL_Tick_t timeout_ms) {
    (void)timeout_ms;
    if (q->tail == q->head) return OSAL_ERR_EMPTY;
    (void)memcpy(item, q->buf + (q->tail * q->item_size), q->item_size);
    q->tail = (uint16_t)((q->tail + 1U) % q->depth);
    return OSAL_OK;
}

static inline OSAL_Status_t OSAL_Queue_Peek(OSAL_Queue_t *q, void *item) {
    if (q->tail == q->head) return OSAL_ERR_EMPTY;
    (void)memcpy(item, q->buf + (q->tail * q->item_size), q->item_size);
    return OSAL_OK;
}

static inline uint16_t OSAL_Queue_Count(OSAL_Queue_t *q) {
    return (uint16_t)((q->head - q->tail + q->depth) % q->depth);
}

#endif /* OSAL_QUEUE */


/* =========================================================================
 * ░░  MUTEX
 * =========================================================================
 * OSAL_MUTEX_DEF(name)
 * OSAL_Mutex_Init(&m)
 * OSAL_Mutex_Lock(&m, timeout_ms)
 * OSAL_Mutex_Unlock(&m)
 * ========================================================================= */
#if defined(OSAL_FREERTOS)

typedef struct { SemaphoreHandle_t handle; } OSAL_Mutex_t;
#define OSAL_MUTEX_DEF(name) static OSAL_Mutex_t name = {NULL}
static inline OSAL_Status_t OSAL_Mutex_Init(OSAL_Mutex_t *m) {
    m->handle = xSemaphoreCreateMutex(); return OSAL_OK; }
static inline OSAL_Status_t OSAL_Mutex_Lock(OSAL_Mutex_t *m, OSAL_Tick_t ticks) {
    return (xSemaphoreTake(m->handle, ticks) == pdTRUE) ? OSAL_OK : OSAL_ERR_TIMEOUT; }
static inline OSAL_Status_t OSAL_Mutex_Unlock(OSAL_Mutex_t *m) {
    xSemaphoreGive(m->handle); return OSAL_OK; }

#elif defined(OSAL_THREADX)

typedef struct { TX_MUTEX handle; } OSAL_Mutex_t;
#define OSAL_MUTEX_DEF(name) static OSAL_Mutex_t name
static inline OSAL_Status_t OSAL_Mutex_Init(OSAL_Mutex_t *m) {
    tx_mutex_create(&m->handle, "m", TX_NO_INHERIT); return OSAL_OK; }
static inline OSAL_Status_t OSAL_Mutex_Lock(OSAL_Mutex_t *m, OSAL_Tick_t ticks) {
    return (tx_mutex_get(&m->handle, ticks) == TX_SUCCESS) ? OSAL_OK : OSAL_ERR_TIMEOUT; }
static inline OSAL_Status_t OSAL_Mutex_Unlock(OSAL_Mutex_t *m) {
    tx_mutex_put(&m->handle); return OSAL_OK; }

#else /* OSAL_BAREMETAL: disable interrupts for critical section */

typedef struct { volatile uint8_t locked; } OSAL_Mutex_t;
#define OSAL_MUTEX_DEF(name) static OSAL_Mutex_t name = {0U}
static inline OSAL_Status_t OSAL_Mutex_Init(OSAL_Mutex_t *m) { m->locked = 0U; return OSAL_OK; }
static inline OSAL_Status_t OSAL_Mutex_Lock(OSAL_Mutex_t *m, OSAL_Tick_t t) {
    (void)t; m->locked = 1U; return OSAL_OK; }
static inline OSAL_Status_t OSAL_Mutex_Unlock(OSAL_Mutex_t *m) {
    m->locked = 0U; return OSAL_OK; }

#endif /* OSAL_MUTEX */


/* =========================================================================
 * ░░  SOFTWARE TIMER
 * =========================================================================
 * OSAL_TIMER_DEF(name)
 * OSAL_Timer_Init(&t, period_ms, repeat, callback, arg)
 * OSAL_Timer_Start(&t)
 * OSAL_Timer_Stop(&t)
 * OSAL_Timer_Reset(&t)   – restart countdown
 * OSAL_Timer_Tick(&t, now_ms) – call from tick ISR or scheduler (bare-metal only)
 * ========================================================================= */
typedef void (*OSAL_TimerCb_t)(void *arg);

#if defined(OSAL_FREERTOS)

typedef struct { TimerHandle_t handle; } OSAL_Timer_t;
#define OSAL_TIMER_DEF(name) static OSAL_Timer_t name = {NULL}
static inline OSAL_Status_t OSAL_Timer_Init(OSAL_Timer_t *t, uint32_t period_ms, bool repeat,
                                             OSAL_TimerCb_t cb, void *arg) {
    t->handle = xTimerCreate("t", pdMS_TO_TICKS(period_ms), repeat ? pdTRUE : pdFALSE,
                              arg, (TimerCallbackFunction_t)cb);
    return OSAL_OK;
}
static inline OSAL_Status_t OSAL_Timer_Start(OSAL_Timer_t *t)  { xTimerStart(t->handle,0); return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Stop(OSAL_Timer_t *t)   { xTimerStop(t->handle,0);  return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Reset(OSAL_Timer_t *t)  { xTimerReset(t->handle,0); return OSAL_OK; }
static inline void           OSAL_Timer_Tick(OSAL_Timer_t *t, uint32_t now_ms) { (void)t; (void)now_ms; }

#elif defined(OSAL_THREADX)

typedef struct { TX_TIMER handle; uint32_t period_ms; OSAL_TimerCb_t cb; void *arg; } OSAL_Timer_t;
#define OSAL_TIMER_DEF(name) static OSAL_Timer_t name
static inline OSAL_Status_t OSAL_Timer_Init(OSAL_Timer_t *t, uint32_t period_ms, bool repeat,
                                             OSAL_TimerCb_t cb, void *arg) {
    t->period_ms = period_ms; t->cb = cb; t->arg = arg;
    tx_timer_create(&t->handle, "t", (void(*)(ULONG))cb, (ULONG)arg,
                    period_ms, repeat ? period_ms : 0, TX_NO_ACTIVATE);
    return OSAL_OK;
}
static inline OSAL_Status_t OSAL_Timer_Start(OSAL_Timer_t *t)  { tx_timer_activate(&t->handle); return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Stop(OSAL_Timer_t *t)   { tx_timer_deactivate(&t->handle); return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Reset(OSAL_Timer_t *t)  { tx_timer_change(&t->handle, t->period_ms, 0); tx_timer_activate(&t->handle); return OSAL_OK; }
static inline void           OSAL_Timer_Tick(OSAL_Timer_t *t, uint32_t now_ms) { (void)t; (void)now_ms; }

#else /* OSAL_BAREMETAL */

typedef struct {
    uint32_t       period_ms;
    uint32_t       expire_ms;   /* absolute ms when timer fires */
    bool           repeat;
    bool           running;
    OSAL_TimerCb_t cb;
    void          *arg;
} OSAL_Timer_t;

#define OSAL_TIMER_DEF(name) static OSAL_Timer_t name = {0U,0U,false,false,NULL,NULL}

static inline OSAL_Status_t OSAL_Timer_Init(OSAL_Timer_t *t, uint32_t period_ms, bool repeat,
                                             OSAL_TimerCb_t cb, void *arg) {
    t->period_ms = period_ms; t->repeat = repeat;
    t->cb = cb; t->arg = arg; t->running = false;
    return OSAL_OK;
}
static inline OSAL_Status_t OSAL_Timer_Start(OSAL_Timer_t *t)  { t->running = true;  return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Stop(OSAL_Timer_t *t)   { t->running = false; return OSAL_OK; }
static inline OSAL_Status_t OSAL_Timer_Reset(OSAL_Timer_t *t)  { t->running = true;  return OSAL_OK; }

/* Call this from the main scheduler tick with current uptime in ms */
static inline void OSAL_Timer_Tick(OSAL_Timer_t *t, uint32_t now_ms) {
    if (!t->running) return;
    if (now_ms >= t->expire_ms) {
        if (t->cb) t->cb(t->arg);
        if (t->repeat) { t->expire_ms = now_ms + t->period_ms; }
        else           { t->running = false; }
    }
}

static inline void OSAL_Timer_SetExpiry(OSAL_Timer_t *t, uint32_t now_ms) {
    t->expire_ms = now_ms + t->period_ms;
}

#endif /* OSAL_TIMER */


/* =========================================================================
 * ░░  TASK CREATION MACROS (RTOS targets only)
 * =========================================================================
 * OSAL_TASK_CREATE(fn, name, stack_words, priority, arg, handle_ptr)
 * ========================================================================= */
#if defined(OSAL_FREERTOS)
#define OSAL_TASK_CREATE(fn, name_, stack_w, prio, arg_, handle_ptr_) \
    xTaskCreate((fn), (name_), (stack_w), (arg_), (prio), (handle_ptr_))

#elif defined(OSAL_THREADX)
/* ThreadX: caller must provide stack buffer */
#define OSAL_TASK_CREATE(fn, name_, stack_buf, stack_sz, prio, arg_, handle_ptr_) \
    tx_thread_create((handle_ptr_), (name_), (void(*)(ULONG))(fn), (ULONG)(arg_), \
                     (stack_buf), (stack_sz), (prio), (prio), TX_NO_TIME_SLICE, TX_AUTO_START)
#else
/* Bare-metal: tasks are registered via Scheduler_AddTask() — see scheduler.h */
#define OSAL_TASK_CREATE(fn, name_, stack_w, prio, arg_, handle_ptr_) \
    Scheduler_AddTask((fn), (name_), (prio), (arg_), (handle_ptr_))
#endif

#endif /* OSAL_H */
