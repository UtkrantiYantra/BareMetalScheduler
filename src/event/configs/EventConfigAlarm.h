/**
 * @file EventConfigAlarm.h
 * @brief Events published by AlarmEventManager
 *
 * @EVENT_MODULE: Alarm
 * @BASE_ID: 0x0400
 */

#ifndef EVENT_CONFIG_ALARM_H
#define EVENT_CONFIG_ALARM_H


/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_ALARM_RAISED                         0x0400U
#define EVT_ALARM_CLEARED                        0x0401U
#define EVT_ALARM_ACKNOWLEDGED                   0x0402U
#define EVT_ALARM_ESCALATED                      0x0403U
#define EVT_ALARM_INHIBITED                      0x0404U
#define EVT_ALARM_SHELVED                        0x0405U
#define EVT_ALARM_LOG_OVERFLOW                   0x0406U

typedef enum {
    ALARM_SEV_INFO    = 0U,
    ALARM_SEV_LOW     = 1U,
    ALARM_SEV_MEDIUM  = 2U,
    ALARM_SEV_HIGH    = 3U,
    ALARM_SEV_CRITICAL= 4U,
} AlarmSeverity_t;




typedef struct {
    uint16_t         alarm_id;        /**< Unique alarm instance ID             */
    uint16_t         source_rule_id;  /**< Rule engine rule that triggered it   */
    uint8_t          sensor_id;       /**< Originating sensor (0 = none)        */
    AlarmSeverity_t  severity;
    int32_t          trigger_value;   /**< Sensor value that caused the alarm   */
    uint32_t         timestamp_ms;
} AlarmPayload_t;

#endif /* EVENT_CONFIG_ALARM_H */
