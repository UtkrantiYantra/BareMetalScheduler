/**
 * @file EventConfigLogging.h
 * @brief Events published by LoggingModule
 *
 * @EVENT_MODULE: Logging
 * @BASE_ID: 0x0600
 */

#ifndef EVENT_CONFIG_LOGGING_H
#define EVENT_CONFIG_LOGGING_H



/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_LOG_ENTRY_WRITTEN                    0x0600U
#define EVT_LOG_BUFFER_FULL                      0x0601U
#define EVT_LOG_HANDLER_ERROR                    0x0602U
#define EVT_LOG_FLUSH_DONE                       0x0603U

typedef struct {
    uint8_t  handler_id;
    uint32_t entries_written;
    uint32_t entries_dropped;
    uint32_t timestamp_ms;
} LogStatusPayload_t;

#endif /* EVENT_CONFIG_LOGGING_H */
