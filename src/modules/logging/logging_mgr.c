/**
 * @file logging_mgr.c
 * @brief Logging Module implementation
 */

#include "logging_mgr.h"
#include "EventConfigLogging.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */
#ifndef LOG_RING_SIZE
#define LOG_RING_SIZE  128U    /* entries in RAM ring buffer */
#endif

#ifndef LOG_MAX_HANDLERS
#define LOG_MAX_HANDLERS  8U
#endif

/* =========================================================================
 * Internal state
 * ========================================================================= */
static struct {
    LogEntry_t   ring[LOG_RING_SIZE];
    uint32_t     head;         /* write index */
    uint32_t     tail;         /* read index for deferred dispatch */
    uint32_t     count;        /* entries currently in ring */
    uint32_t     total_written;
    uint32_t     total_dropped;

    LogHandler_t *handlers[LOG_MAX_HANDLERS];
    uint8_t       handler_count;

    bool initialized;
} s_log;

/* =========================================================================
 * Platform handler stubs (weak — override in platform code)
 * ========================================================================= */
__attribute__((weak)) void platform_log_uart(const char *text, uint16_t len) {
    (void)len;
    printf("%s", text);
}
__attribute__((weak)) void platform_log_rtt(const char *text, uint16_t len) {
    (void)len; (void)text;
    /* Implement: SEGGER_RTT_WriteString(0, text); */
}
__attribute__((weak)) bool platform_log_flash(const uint8_t *data, uint16_t len) {
    (void)data; (void)len;
    return true; /* stub: implement flash ring write */
}
__attribute__((weak)) bool platform_log_sd(const uint8_t *data, uint16_t len) {
    (void)data; (void)len;
    return true; /* stub: implement SD/FatFS write */
}

/* =========================================================================
 * Severity string
 * ========================================================================= */
static const char *sev_str(LogSeverity_t s) {
    switch(s) {
        case LOG_SEV_DEBUG:    return "DBG";
        case LOG_SEV_INFO:     return "INF";
        case LOG_SEV_WARN:     return "WRN";
        case LOG_SEV_HIGH:     return "HGH";
        case LOG_SEV_CRITICAL: return "CRT";
        default:               return "???";
    }
}

/* =========================================================================
 * Format entry to text (for UART/RTT handlers)
 * ========================================================================= */
static void format_entry(const LogEntry_t *e, char *buf, uint16_t buf_len)
{
    int n = 0;
    n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                  "[%8lu][%s] ", (unsigned long)e->timestamp_ms, sev_str(e->severity));

    switch (e->type) {
        case LOG_TYPE_TEXT:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "<%s> %s\n", e->u.text.tag, e->u.text.msg);
            break;
        case LOG_TYPE_SENSOR:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "SENSOR id=%u val=%ld src=%u\n",
                          e->u.sensor.sensor_id,
                          (long)e->u.sensor.value_x100,
                          e->u.sensor.source);
            break;
        case LOG_TYPE_EVENT:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "EVENT  id=0x%04X len=%u\n",
                          e->u.event.event_id, e->u.event.len);
            break;
        case LOG_TYPE_ALARM:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "ALARM  id=%u sev=%u val=%ld\n",
                          e->u.alarm.alarm_id,
                          e->u.alarm.severity,
                          (long)e->u.alarm.trigger_val);
            break;
        case LOG_TYPE_RELAY:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "RELAY  id=%u state=%u prio=%u\n",
                          e->u.relay.relay_id,
                          e->u.relay.state,
                          e->u.relay.priority);
            break;
        case LOG_TYPE_DIAG:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "DIAG   loops=%lu evals=%lu errs=%lu\n",
                          (unsigned long)e->u.diag.loop_count,
                          (unsigned long)e->u.diag.rule_evals,
                          (unsigned long)e->u.diag.errs);
            break;
        case LOG_TYPE_RAW:
            n += snprintf(buf + n, (size_t)(buf_len - (uint16_t)n),
                          "RAW    len=%u\n", e->u.raw.len);
            break;
        default:
            break;
    }
    (void)n;
}

/* =========================================================================
 * Dispatch one entry to a handler
 * ========================================================================= */
static void dispatch_to_handler(LogHandler_t *h, const LogEntry_t *e)
{
    if (!h->enabled) return;
    if (e->severity < h->min_severity) return;
    if (!(h->type_mask & (uint8_t)e->type)) return;

    switch (h->type) {
        case LOG_HANDLER_UART:
        case LOG_HANDLER_RTT: {
            char buf[LOG_TEXT_MAX + 64];
            format_entry(e, buf, (uint16_t)sizeof(buf));
            if (h->type == LOG_HANDLER_UART) platform_log_uart(buf, (uint16_t)strlen(buf));
            else                              platform_log_rtt(buf,  (uint16_t)strlen(buf));
            h->entries_written++;
            break;
        }
        case LOG_HANDLER_FLASH:
            if (platform_log_flash((const uint8_t*)e, (uint16_t)sizeof(*e)))
                h->entries_written++;
            else
                h->entries_dropped++;
            break;
        case LOG_HANDLER_SD:
            if (platform_log_sd((const uint8_t*)e, (uint16_t)sizeof(*e)))
                h->entries_written++;
            else
                h->entries_dropped++;
            break;
        case LOG_HANDLER_CALLBACK:
            if (h->callback) {
                h->callback(e, h->callback_arg);
                h->entries_written++;
            }
            break;
        default:
            break;
    }
}

/* =========================================================================
 * Init
 * ========================================================================= */
void LogMgr_Init(void)
{
    (void)memset(&s_log, 0, sizeof(s_log));
    s_log.initialized = true;
}

/* =========================================================================
 * Add handler
 * ========================================================================= */
uint8_t LogMgr_AddHandler(LogHandler_t *handler)
{
    if (!handler || s_log.handler_count >= LOG_MAX_HANDLERS) return 0xFFU;
    handler->id = s_log.handler_count;
    s_log.handlers[s_log.handler_count++] = handler;
    return handler->id;
}

void LogMgr_SetHandlerEnabled(uint8_t id, bool enabled)
{
    if (id < s_log.handler_count && s_log.handlers[id])
        s_log.handlers[id]->enabled = enabled;
}

/* =========================================================================
 * Write (ISR-safe: we only advance head after copy)
 * ========================================================================= */
bool LogMgr_Write(const LogEntry_t *entry)
{
    if (!s_log.initialized || !entry) return false;

    if (s_log.count >= LOG_RING_SIZE) {
        /* Ring full: overwrite oldest entry (wrap) */
        s_log.tail = (s_log.tail + 1U) % LOG_RING_SIZE;
        s_log.count--;
        s_log.total_dropped++;
        LogStatusPayload_t sp = { 0U, s_log.total_written, s_log.total_dropped,
                                   Scheduler_GetTick() };
        EVENT_PUBLISH(EVT_LOG_BUFFER_FULL, &sp, sizeof(sp));
    }

    (void)memcpy(&s_log.ring[s_log.head], entry, sizeof(*entry));
    s_log.head = (s_log.head + 1U) % LOG_RING_SIZE;
    s_log.count++;
    s_log.total_written++;

    /* Immediate dispatch for UART/RTT (blocking ok in task context) */
    uint8_t i;
    for (i = 0U; i < s_log.handler_count; i++) {
        LogHandler_t *h = s_log.handlers[i];
        if (h->type == LOG_HANDLER_UART || h->type == LOG_HANDLER_RTT ||
            h->type == LOG_HANDLER_CALLBACK) {
            dispatch_to_handler(h, entry);
        }
    }

    return true;
}

/* =========================================================================
 * Deferred task — dispatch to flash/SD handlers
 * ========================================================================= */
void LogMgr_Task(void *arg)
{
    (void)arg;
    /* Drain ring for deferred handlers (flash, SD) */
    uint8_t h;
    uint32_t dispatched = 0U;

    while (s_log.tail != s_log.head && dispatched < 8U) {
        const LogEntry_t *e = &s_log.ring[s_log.tail];
        for (h = 0U; h < s_log.handler_count; h++) {
            if (s_log.handlers[h]->type == LOG_HANDLER_FLASH ||
                s_log.handlers[h]->type == LOG_HANDLER_SD) {
                dispatch_to_handler(s_log.handlers[h], e);
            }
        }
        s_log.tail = (s_log.tail + 1U) % LOG_RING_SIZE;
        dispatched++;
    }
}

/* =========================================================================
 * Flush
 * ========================================================================= */
void LogMgr_Flush(void)
{
    uint8_t h;
    for (h = 0U; h < s_log.handler_count; h++) {
        /* Nothing special needed for UART/callback — already immediate.
         * Platform flash/SD flush can be implemented here via weak function. */
        (void)s_log.handlers[h];
    }
    LogStatusPayload_t sp = { 0U, s_log.total_written, s_log.total_dropped,
                               Scheduler_GetTick() };
    EVENT_PUBLISH(EVT_LOG_FLUSH_DONE, &sp, sizeof(sp));
}

/* =========================================================================
 * Typed readers
 * ========================================================================= */
uint32_t LogMgr_ReadAll(LogReadCb_t cb, void *arg, uint8_t filter)
{
    uint32_t visited = 0U;
    uint32_t idx     = s_log.tail;
    uint32_t n       = s_log.count;

    while (n > 0U) {
        const LogEntry_t *e = &s_log.ring[idx];
        if (filter & (uint8_t)e->type) {
            if (!cb(e, arg)) break;
            visited++;
        }
        idx = (idx + 1U) % LOG_RING_SIZE;
        n--;
    }
    return visited;
}

uint32_t LogMgr_ReadSensor(uint8_t sid, LogReadCb_t cb, void *arg)
{
    uint32_t visited = 0U;
    uint32_t idx     = s_log.tail;
    uint32_t n       = s_log.count;

    while (n > 0U) {
        const LogEntry_t *e = &s_log.ring[idx];
        if (e->type == LOG_TYPE_SENSOR &&
            (sid == 0U || e->u.sensor.sensor_id == sid)) {
            if (!cb(e, arg)) break;
            visited++;
        }
        idx = (idx + 1U) % LOG_RING_SIZE;
        n--;
    }
    return visited;
}

uint32_t LogMgr_ReadAlarm(uint16_t alarm_id, LogReadCb_t cb, void *arg)
{
    uint32_t visited = 0U;
    uint32_t idx     = s_log.tail;
    uint32_t n       = s_log.count;

    while (n > 0U) {
        const LogEntry_t *e = &s_log.ring[idx];
        if (e->type == LOG_TYPE_ALARM &&
            (alarm_id == 0U || e->u.alarm.alarm_id == alarm_id)) {
            if (!cb(e, arg)) break;
            visited++;
        }
        idx = (idx + 1U) % LOG_RING_SIZE;
        n--;
    }
    return visited;
}

uint32_t LogMgr_ReadText(const char *tag_prefix, LogReadCb_t cb, void *arg)
{
    uint32_t visited = 0U;
    uint32_t idx     = s_log.tail;
    uint32_t n       = s_log.count;
    size_t   plen    = tag_prefix ? strlen(tag_prefix) : 0U;

    while (n > 0U) {
        const LogEntry_t *e = &s_log.ring[idx];
        if (e->type == LOG_TYPE_TEXT) {
            if (plen == 0U || strncmp(e->u.text.tag, tag_prefix, plen) == 0) {
                if (!cb(e, arg)) break;
                visited++;
            }
        }
        idx = (idx + 1U) % LOG_RING_SIZE;
        n--;
    }
    return visited;
}

void LogMgr_Clear(void)
{
    s_log.head = 0U; s_log.tail = 0U; s_log.count = 0U;
}

uint32_t LogMgr_Count(void) { return s_log.count; }

void LogMgr_GetStats(LogMgrStats_t *out)
{
    if (!out) return;
    out->total_written = s_log.total_written;
    out->total_dropped = s_log.total_dropped;
    out->ring_count    = s_log.count;
    out->handler_count = s_log.handler_count;
}

/* =========================================================================
 * WriteText convenience (variadic)
 * ========================================================================= */
void LogMgr_WriteText(LogSeverity_t sev, const char *tag, const char *fmt, ...)
{
    LogEntry_t e;
    (void)memset(&e, 0, sizeof(e));
    e.timestamp_ms = Scheduler_GetTick();
    e.type         = LOG_TYPE_TEXT;
    e.severity     = sev;

    if (tag) {
        (void)strncpy(e.u.text.tag, tag, LOG_TAG_MAX - 1U);
        e.u.text.tag[LOG_TAG_MAX - 1U] = '\0';
    }

    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(e.u.text.msg, LOG_TEXT_MAX, fmt, ap);
    va_end(ap);

    (void)LogMgr_Write(&e);
}
