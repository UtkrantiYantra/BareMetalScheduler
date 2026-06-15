/**
 * @file re_types.h
 * @brief Core type definitions for the Rule Engine Framework
 *
 * Production-grade configuration-driven firmware framework.
 * Supports STM32, ESP32, and Linux simulation.
 * MISRA-C 2012 friendly. No dynamic memory allocation.
 *
 * @version 1.0.0
 */

#ifndef RE_TYPES_H
#define RE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Platform detection
 * ========================================================================= */
#if defined(STM32_PLATFORM)
    #include "stm32_hal_bridge.h"
#elif defined(ESP32_PLATFORM)
    #include "esp32_hal_bridge.h"
#else
    /* Linux simulation - default */
    #include <time.h>
#endif

/* =========================================================================
 * Version
 * ========================================================================= */
#define RE_FRAMEWORK_VERSION_MAJOR  (1U)
#define RE_FRAMEWORK_VERSION_MINOR  (0U)
#define RE_FRAMEWORK_VERSION_PATCH  (0U)

/* =========================================================================
 * Limits (compile-time, must match config.h generated values)
 * ========================================================================= */
#define RE_MAX_SENSORS              (99U)
#define RE_MAX_OUTPUTS              (99U)
#define RE_MAX_RULES                (1000U)
#define RE_MAX_CONDITIONS_PER_RULE  (8U)
#define RE_MAX_ACTIONS_PER_RULE     (8U)
#define RE_MAX_SCHEDULES            (32U)
#define RE_MAX_SCHEDULE_SLOTS       (8U)
#define RE_MAX_LOG_ENTRIES          (256U)
#define RE_MAX_NAME_LEN             (32U)
#define RE_MAX_UNIT_LEN             (16U)

/* =========================================================================
 * Return codes
 * ========================================================================= */
typedef enum
{
    RE_OK                   = 0,
    RE_ERR_NULL_PTR         = -1,
    RE_ERR_INVALID_ID       = -2,
    RE_ERR_INVALID_PARAM    = -3,
    RE_ERR_DISABLED         = -4,
    RE_ERR_RANGE            = -5,
    RE_ERR_TIMEOUT          = -6,
    RE_ERR_FULL             = -7,
    RE_ERR_NOT_FOUND        = -8,
    RE_ERR_HAL              = -9
} RE_Status_t;

/* =========================================================================
 * Sensor types
 * ========================================================================= */
typedef enum
{
    SENSOR_TYPE_TEMPERATURE     = 0,
    SENSOR_TYPE_HUMIDITY        = 1,
    SENSOR_TYPE_CURRENT         = 2,
    SENSOR_TYPE_VOLTAGE         = 3,
    SENSOR_TYPE_FIRE            = 4,
    SENSOR_TYPE_SMOKE           = 5,
    SENSOR_TYPE_GAS             = 6,
    SENSOR_TYPE_ULTRASONIC      = 7,
    SENSOR_TYPE_WATER_LEVEL     = 8,
    SENSOR_TYPE_PRESSURE        = 9,
    SENSOR_TYPE_DOOR            = 10,
    SENSOR_TYPE_PIR_MOTION      = 11,
    SENSOR_TYPE_DIGITAL         = 12,
    SENSOR_TYPE_VIRTUAL         = 13,
    SENSOR_TYPE_RTC_TIME        = 14,
    SENSOR_TYPE_TOU_SCHEDULE    = 15,
    SENSOR_TYPE_MAX
} SensorType_t;

/* =========================================================================
 * Output types
 * ========================================================================= */
typedef enum
{
    OUTPUT_TYPE_RELAY           = 0,
    OUTPUT_TYPE_LED             = 1,
    OUTPUT_TYPE_ALARM           = 2,
    OUTPUT_TYPE_FIRE_ALARM      = 3,
    OUTPUT_TYPE_BUZZER          = 4,
    OUTPUT_TYPE_HOOTER          = 5,
    OUTPUT_TYPE_PUMP            = 6,
    OUTPUT_TYPE_FAN             = 7,
    OUTPUT_TYPE_SIREN           = 8,
    OUTPUT_TYPE_SOLENOID_VALVE  = 9,
    OUTPUT_TYPE_CONTACTOR       = 10,
    OUTPUT_TYPE_MAX
} OutputType_t;

/* =========================================================================
 * Comparison / logical operators
 * ========================================================================= */
typedef enum
{
    OP_EQ           = 0,
    OP_NE           = 1,
    OP_GT           = 2,
    OP_GTE          = 3,
    OP_LT           = 4,
    OP_LTE          = 5,
    OP_BETWEEN      = 6,
    OP_NOT_BETWEEN  = 7,
    OP_ON           = 8,
    OP_OFF          = 9,
    OP_MAX
} CompareOp_t;

typedef enum
{
    LOGIC_AND   = 0,
    LOGIC_OR    = 1,
    LOGIC_NOT   = 2,
    LOGIC_XOR   = 3
} LogicOp_t;

/* =========================================================================
 * Output action types
 * ========================================================================= */
typedef enum
{
    ACTION_ON       = 0,
    ACTION_OFF      = 1,
    ACTION_TOGGLE   = 2,
    ACTION_PULSE    = 3
} ActionType_t;

/* =========================================================================
 * Sensor value union — fixed-point integer scaled by 100 for float sensors
 * e.g., 25.34°C stored as int32 2534 (divide by 100 for display)
 * ========================================================================= */
typedef int32_t SensorValue_t;   /* scaled x100 for analogue, raw for digital */

/* Convenience macros */
#define RE_FLOAT_TO_VAL(f)   ((SensorValue_t)((f) * 100.0f))
#define RE_VAL_TO_FLOAT(v)   ((float)(v) / 100.0f)

/* =========================================================================
 * Timestamp — seconds since epoch (or RTC ticks)
 * ========================================================================= */
typedef uint32_t RE_Timestamp_t;

/* =========================================================================
 * Sensor state
 * ========================================================================= */
typedef struct
{
    SensorValue_t   value;           /**< Current scaled value              */
    SensorValue_t   prev_value;      /**< Previous value (hysteresis)       */
    RE_Timestamp_t  last_sampled_ts; /**< Timestamp of last successful read */
    bool            valid;           /**< Data is fresh and valid           */
    bool            alarm_active;    /**< Hysteresis alarm state (current tick) */
    bool            prev_alarm_active; /**< Alarm state from previous tick  */
    uint32_t        sample_count;    /**< Total samples taken               */
    uint32_t        error_count;     /**< Consecutive read errors           */
} SensorState_t;

/* =========================================================================
 * Output state
 * ========================================================================= */
typedef enum
{
    OUTPUT_STATE_OFF        = 0,
    OUTPUT_STATE_ON         = 1,
    OUTPUT_STATE_PULSING    = 2,
    OUTPUT_STATE_FAULT      = 3
} OutputPhysState_t;

typedef struct
{
    OutputPhysState_t   phys_state;         /**< Current physical state    */
    uint8_t             active_priority;    /**< Priority of current owner */
    uint16_t            owner_rule_id;      /**< Rule that set state       */
    RE_Timestamp_t      pulse_end_ts;       /**< Pulse expiry timestamp    */
    uint32_t            on_count;           /**< Times turned ON           */
    uint32_t            toggle_count;       /**< Times toggled             */
    RE_Timestamp_t      last_change_ts;     /**< Last state change time    */
} OutputState_t;

/* =========================================================================
 * RTC / Time structure
 * ========================================================================= */
typedef struct
{
    uint8_t  hour;       /**< 0-23  */
    uint8_t  minute;     /**< 0-59  */
    uint8_t  second;     /**< 0-59  */
    uint8_t  day;        /**< 1-31  */
    uint8_t  month;      /**< 1-12  */
    uint16_t year;       /**< e.g. 2024 */
    uint8_t  weekday;    /**< 0=Sun, 1=Mon ... 6=Sat */
} RE_RTC_t;

/* =========================================================================
 * Schedule slot (TOU)
 * ========================================================================= */
typedef enum
{
    SCHED_DAILY     = 0,
    SCHED_WEEKLY    = 1,
    SCHED_WEEKEND   = 2,
    SCHED_HOLIDAY   = 3,
    SCHED_SEASONAL  = 4
} ScheduleType_t;

typedef struct
{
    uint8_t start_hour;     /**< Slot start hour (0-23)  */
    uint8_t start_min;      /**< Slot start minute       */
    uint8_t end_hour;       /**< Slot end hour           */
    uint8_t end_min;        /**< Slot end minute         */
    uint8_t weekday_mask;   /**< Bit mask: bit0=Sun..bit6=Sat (SCHED_WEEKLY) */
} ScheduleSlot_t;

/* =========================================================================
 * Event log entry
 * ========================================================================= */
typedef struct
{
    RE_Timestamp_t  timestamp;
    uint16_t        rule_id;
    uint8_t         sensor_id;
    SensorValue_t   sensor_value;
    uint8_t         output_id;
    ActionType_t    action;
} RE_LogEntry_t;

/* =========================================================================
 * Diagnostics
 * ========================================================================= */
typedef struct
{
    uint32_t    rule_eval_count;
    uint32_t    rule_fire_count;
    uint32_t    sensor_read_ok;
    uint32_t    sensor_read_err;
    uint32_t    output_write_ok;
    uint32_t    output_write_err;
    uint32_t    loop_count;
    uint32_t    last_loop_duration_us;
    uint16_t    last_rule_fired;
    RE_Timestamp_t last_rule_time;
} RE_Diagnostics_t;

#endif /* RE_TYPES_H */
