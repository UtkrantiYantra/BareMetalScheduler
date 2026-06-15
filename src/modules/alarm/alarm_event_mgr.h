/**
 * @file alarm_event_mgr.h
 * @brief Alarm Event Manager
 *
 * Subscribes to EVT_ANALOG_THRESHOLD_* and EVT_DIGITAL_RISING_EDGE events
 * from other modules, maps them to alarm records, and publishes AlarmPayload_t
 * events (RAISED, CLEARED, ACKNOWLEDGED, ESCALATED).
 *
 * Alarm table: static array of AlarmRecord_t.
 *
 * Usage:
 *   1. Define alarm rules with ALARM_DEF macro.
 *   2. Call AlarmEventMgr_Init() and AlarmEventMgr_AddAlarm() during startup.
 *   3. AlarmEventMgr_Task() handles escalation timeouts.
 *   4. Call AlarmEventMgr_Acknowledge(alarm_id) on operator ACK.
 */

#ifndef ALARM_EVENT_MGR_H
#define ALARM_EVENT_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"
#include "EventConfigAlarm.h"
#include "EventConfigAnalogSensor.h"

typedef enum {
    ALARM_STATE_INACTIVE     = 0U,
    ALARM_STATE_ACTIVE       = 1U,
    ALARM_STATE_ACKNOWLEDGED = 2U,
    ALARM_STATE_ESCALATED    = 3U,
    ALARM_STATE_INHIBITED    = 4U,
} AlarmState_t;

typedef struct {
    uint16_t        id;
    const char     *name;
    EventID_t       trigger_event;   /**< EVT_* that raises this alarm         */
    uint8_t         sensor_id;       /**< Sensor associated (0=any)            */
    AlarmSeverity_t severity;
    uint32_t        ack_timeout_ms;  /**< Escalate if unacked after this ms    */
    bool            latching;        /**< true = stays active until ACK        */
    bool            enabled;
} AlarmDesc_t;

typedef struct {
    AlarmState_t state;
    uint32_t     raised_ms;
    uint32_t     acked_ms;
    int32_t      trigger_value;
    uint32_t     raise_count;
} AlarmRuntimeState_t;

#define ALARM_DEF(var, id_, name_, trigger_, sensor_, sev_, ack_to_, latch_) \
    static AlarmDesc_t var = {                                                 \
        (id_), (name_), (trigger_), (sensor_), (sev_),                        \
        (ack_to_), (latch_), true                                              \
    }

/* API */
void AlarmEventMgr_Init(void);
bool AlarmEventMgr_AddAlarm(AlarmDesc_t *desc, AlarmRuntimeState_t *state);
void AlarmEventMgr_Acknowledge(uint16_t alarm_id);
void AlarmEventMgr_Inhibit(uint16_t alarm_id, bool inhibit);
AlarmState_t AlarmEventMgr_GetState(uint16_t alarm_id);
void AlarmEventMgr_Task(void *arg);   /* handles escalation timeouts */

#endif /* ALARM_EVENT_MGR_H */
