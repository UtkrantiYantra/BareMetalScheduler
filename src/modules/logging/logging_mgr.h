/**
 * @file logging_mgr.h
 * @brief Logging Module — multi-handler, typed capture objects, typed readers
 *
 * Design
 * ------
 * The logging module stores typed log entries in a RAM ring buffer and
 * dispatches them to one or more handlers.  Each handler targets a different
 * storage or output medium.
 *
 * Built-in handler types:
 *   LOG_HANDLER_UART     — UART/printf text output
 *   LOG_HANDLER_RTT      — Segger RTT (embedded debug)
 *   LOG_HANDLER_FLASH    — Raw binary to on-chip flash ring
 *   LOG_HANDLER_SD       — FAT file on SD card
 *   LOG_HANDLER_CALLBACK — Call user function (test / cloud forwarding)
 *
 * Log entry types (capture objects):
 *   LOG_TYPE_TEXT        — printf-style text string
 *   LOG_TYPE_SENSOR      — sensor ID + scaled value
 *   LOG_TYPE_EVENT       — EventID + payload snapshot
 *   LOG_TYPE_ALARM       — AlarmPayload_t snapshot
 *   LOG_TYPE_RELAY       — RelayPayload_t snapshot
 *   LOG_TYPE_DIAG        — Rule engine diagnostics snapshot
 *   LOG_TYPE_RAW         — Arbitrary binary blob (up to LOG_RAW_MAX bytes)
 *
 * Each handler can filter by:
 *   - Minimum severity (LOG_SEV_DEBUG..LOG_SEV_CRITICAL)
 *   - Log type bitmask (LOG_TYPE_SENSOR | LOG_TYPE_ALARM etc.)
 *
 * Typed reader API:
 *   LogMgr_ReadSensor()  — iterate sensor log entries
 *   LogMgr_ReadAlarm()   — iterate alarm log entries
 *   LogMgr_ReadText()    — iterate text entries
 *   LogMgr_ReadAll()     — iterate all entries (any type)
 *
 * Registration macros:
 *   LOG_HANDLER_DEF(var, type, min_sev, type_mask)
 *   LOG_ADD_HANDLER(handler_ptr)
 *
 * Usage:
 *   LOG_TEXT(LOG_SEV_INFO, "BoilerTemp", "Temp alarm: %.2f C", 55.0f);
 *   LOG_SENSOR(LOG_SEV_WARN, sensor_id, value_x100);
 *   LOG_ALARM(LOG_SEV_HIGH, &alarm_payload);
 */

#ifndef LOGGING_MGR_H
#define LOGGING_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

/* =========================================================================
 * Severity levels
 * ========================================================================= */
typedef enum {
    LOG_SEV_DEBUG    = 0U,
    LOG_SEV_INFO     = 1U,
    LOG_SEV_WARN     = 2U,
    LOG_SEV_HIGH     = 3U,
    LOG_SEV_CRITICAL = 4U,
} LogSeverity_t;

/* =========================================================================
 * Log entry types (capture objects)
 * ========================================================================= */
typedef enum {
    LOG_TYPE_TEXT    = (1U << 0),
    LOG_TYPE_SENSOR  = (1U << 1),
    LOG_TYPE_EVENT   = (1U << 2),
    LOG_TYPE_ALARM   = (1U << 3),
    LOG_TYPE_RELAY   = (1U << 4),
    LOG_TYPE_DIAG    = (1U << 5),
    LOG_TYPE_RAW     = (1U << 6),
    LOG_TYPE_ALL     = 0xFFU,
} LogType_t;

/* =========================================================================
 * Typed capture payloads
 * ========================================================================= */
#define LOG_TEXT_MAX  80U
#define LOG_TAG_MAX   16U
#define LOG_RAW_MAX   48U

typedef struct { char tag[LOG_TAG_MAX]; char msg[LOG_TEXT_MAX]; } LogText_t;
typedef struct { uint8_t sensor_id; int32_t value_x100; uint8_t source; }  LogSensor_t;
typedef struct { EventID_t event_id; uint8_t payload[16]; uint8_t len; }   LogEvent_t;
typedef struct { uint16_t alarm_id; uint8_t severity; int32_t trigger_val;} LogAlarm_t;
typedef struct { uint8_t relay_id;  uint8_t state; uint8_t priority; }     LogRelay_t;
typedef struct { uint32_t loop_count; uint32_t rule_evals; uint32_t errs; } LogDiag_t;
typedef struct { uint8_t len; uint8_t data[LOG_RAW_MAX]; }                  LogRaw_t;

/* =========================================================================
 * Log entry (what lives in the ring buffer)
 * ========================================================================= */
typedef struct {
    uint32_t      timestamp_ms;
    LogType_t     type;
    LogSeverity_t severity;
    union {
        LogText_t   text;
        LogSensor_t sensor;
        LogEvent_t  event;
        LogAlarm_t  alarm;
        LogRelay_t  relay;
        LogDiag_t   diag;
        LogRaw_t    raw;
    } u;
} LogEntry_t;

/* =========================================================================
 * Handler types
 * ========================================================================= */
typedef enum {
    LOG_HANDLER_UART     = 0U,
    LOG_HANDLER_RTT      = 1U,
    LOG_HANDLER_FLASH    = 2U,
    LOG_HANDLER_SD       = 3U,
    LOG_HANDLER_CALLBACK = 4U,
} LogHandlerType_t;

typedef void (*LogHandlerCb_t)(const LogEntry_t *entry, void *arg);

typedef struct {
    uint8_t          id;
    LogHandlerType_t type;
    LogSeverity_t    min_severity;
    uint8_t          type_mask;       /**< Bitmask of LogType_t accepted       */
    LogHandlerCb_t   callback;        /**< Used when type == LOG_HANDLER_CALLBACK */
    void            *callback_arg;
    bool             enabled;

    /* Runtime stats */
    uint32_t         entries_written;
    uint32_t         entries_dropped;
} LogHandler_t;

/* =========================================================================
 * LOG_HANDLER_DEF — declare a static handler
 *
 *   LOG_HANDLER_DEF(g_uart_handler, LOG_HANDLER_UART, LOG_SEV_DEBUG, LOG_TYPE_ALL);
 *   LOG_HANDLER_DEF(g_flash_handler, LOG_HANDLER_FLASH, LOG_SEV_WARN,
 *                   LOG_TYPE_SENSOR | LOG_TYPE_ALARM);
 * ========================================================================= */
#define LOG_HANDLER_DEF(var, type_, min_sev_, type_mask_)   \
    static LogHandler_t var = {                              \
        0U, (type_), (min_sev_), (uint8_t)(type_mask_),     \
        NULL, NULL, true, 0U, 0U                             \
    }

/* =========================================================================
 * Manager API
 * ========================================================================= */

/** @brief Initialise logging. Call before any LOG_* macro. */
void LogMgr_Init(void);

/** @brief Register a handler. Returns handler ID, or 0xFF on failure. */
uint8_t LogMgr_AddHandler(LogHandler_t *handler);

/** @brief Enable/disable a handler at runtime. */
void LogMgr_SetHandlerEnabled(uint8_t handler_id, bool enabled);

/**
 * @brief Write a log entry. Called by LOG_* macros.
 * Thread-safe: uses critical section if available.
 */
bool LogMgr_Write(const LogEntry_t *entry);

/** @brief Flush all handlers (block until written). */
void LogMgr_Flush(void);

/** @brief Scheduler task — deferred handler dispatch. */
void LogMgr_Task(void *arg);

/* =========================================================================
 * Typed reader API
 * ========================================================================= */
typedef bool (*LogReadCb_t)(const LogEntry_t *entry, void *arg);

/**
 * @brief Iterate all log entries, calling cb for each.
 * @param cb     Callback; return false to stop iteration.
 * @param arg    Passed to cb.
 * @param filter LogType_t bitmask (LOG_TYPE_ALL for everything).
 * @return Number of entries visited.
 */
uint32_t LogMgr_ReadAll(LogReadCb_t cb, void *arg, uint8_t filter);

/** @brief Iterate only sensor entries. */
uint32_t LogMgr_ReadSensor(uint8_t sensor_id_filter,
                            LogReadCb_t cb, void *arg);

/** @brief Iterate only alarm entries. */
uint32_t LogMgr_ReadAlarm(uint16_t alarm_id_filter,
                           LogReadCb_t cb, void *arg);

/** @brief Iterate only text entries matching tag prefix. */
uint32_t LogMgr_ReadText(const char *tag_prefix,
                          LogReadCb_t cb, void *arg);

/** @brief Clear the in-RAM ring buffer. */
void LogMgr_Clear(void);

/** @brief Return number of entries currently in ring. */
uint32_t LogMgr_Count(void);

/* Module statistics for the diagnostics subsystem */
typedef struct {
    uint32_t total_written;
    uint32_t total_dropped;
    uint32_t ring_count;
    uint8_t  handler_count;
} LogMgrStats_t;

void LogMgr_GetStats(LogMgrStats_t *out);

/* =========================================================================
 * Convenience macros
 * ========================================================================= */
void LogMgr_WriteText(LogSeverity_t sev, const char *tag, const char *fmt, ...);

#define LOG_TEXT(sev, tag, fmt, ...)  LogMgr_WriteText((sev), (tag), (fmt), ##__VA_ARGS__)

#define LOG_SENSOR(sev, sid, val, src)                        \
    do {                                                       \
        LogEntry_t _e = {0};                                   \
        _e.timestamp_ms    = Scheduler_GetTick();              \
        _e.type            = LOG_TYPE_SENSOR;                  \
        _e.severity        = (sev);                            \
        _e.u.sensor.sensor_id  = (sid);                        \
        _e.u.sensor.value_x100 = (val);                        \
        _e.u.sensor.source     = (uint8_t)(src);               \
        (void)LogMgr_Write(&_e);                               \
    } while(0)

#define LOG_ALARM(sev, alarm_payload_ptr)                      \
    do {                                                        \
        LogEntry_t _e = {0};                                    \
        _e.timestamp_ms   = Scheduler_GetTick();               \
        _e.type           = LOG_TYPE_ALARM;                     \
        _e.severity       = (sev);                             \
        _e.u.alarm.alarm_id    = (alarm_payload_ptr)->alarm_id;\
        _e.u.alarm.severity    = (uint8_t)(alarm_payload_ptr)->severity;\
        _e.u.alarm.trigger_val = (alarm_payload_ptr)->trigger_value;    \
        (void)LogMgr_Write(&_e);                               \
    } while(0)

#define LOG_EVENT(sev, eid, pay_ptr, pay_len)                  \
    do {                                                        \
        LogEntry_t _e = {0};                                    \
        _e.timestamp_ms   = Scheduler_GetTick();               \
        _e.type           = LOG_TYPE_EVENT;                     \
        _e.severity       = (sev);                             \
        _e.u.event.event_id = (eid);                            \
        _e.u.event.len      = (uint8_t)(pay_len);              \
        if ((pay_ptr) && (pay_len) > 0)                        \
            (void)memcpy(_e.u.event.payload, (pay_ptr),        \
                         ((pay_len) > 16U ? 16U : (pay_len))); \
        (void)LogMgr_Write(&_e);                               \
    } while(0)

#define LOG_RELAY(sev, rid, state_, prio_)                     \
    do {                                                        \
        LogEntry_t _e = {0};                                    \
        _e.timestamp_ms       = Scheduler_GetTick();           \
        _e.type               = LOG_TYPE_RELAY;                 \
        _e.severity           = (sev);                         \
        _e.u.relay.relay_id   = (rid);                         \
        _e.u.relay.state      = (state_);                      \
        _e.u.relay.priority   = (prio_);                       \
        (void)LogMgr_Write(&_e);                               \
    } while(0)

#include <string.h>  /* memcpy for LOG_EVENT */

#endif /* LOGGING_MGR_H */
