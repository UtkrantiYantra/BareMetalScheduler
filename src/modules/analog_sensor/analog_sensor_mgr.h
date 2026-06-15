/**
 * @file analog_sensor_mgr.h
 * @brief Analog Sensor Manager
 *
 * Manages one or more analog input channels.  Each channel can be read from
 * one of five sources:
 *
 *   ANALOG_SRC_ADC    — Direct ADC peripheral (on-chip)
 *   ANALOG_SRC_I2C    — I2C sensor (e.g. ADS1115, INA219)
 *   ANALOG_SRC_SPI    — SPI sensor (e.g. ADS1248, MAX31865)
 *   ANALOG_SRC_RS485  — Modbus RTU register (RS485 bus)
 *   ANALOG_SRC_UART   — UART ASCII/binary stream injection
 *   ANALOG_SRC_INJECT — Software injection (test / simulation)
 *
 * All sources feed through the same scaling + filtering pipeline and inject
 * the result into the rule engine sensor table via RE_HAL_SimSetSensor()
 * (or the real HAL read path on embedded targets).
 *
 * Each channel publishes EVT_ANALOG_READING_READY on every new value and
 * EVT_ANALOG_THRESHOLD_HIGH/LOW when configured thresholds are crossed.
 *
 * Registration macro:
 *   ANALOG_CHANNEL_DEF(var, id, name, source, hw_addr, scale, offset,
 *                       lo_thresh, hi_thresh, sample_ms)
 */

#ifndef ANALOG_SENSOR_MGR_H
#define ANALOG_SENSOR_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

/* =========================================================================
 * Source types
 * ========================================================================= */
typedef enum {
    ANALOG_SRC_ADC    = 0U,   /**< On-chip ADC                               */
    ANALOG_SRC_I2C    = 1U,   /**< I2C slave device                          */
    ANALOG_SRC_SPI    = 2U,   /**< SPI slave device                          */
    ANALOG_SRC_RS485  = 3U,   /**< Modbus RTU register                       */
    ANALOG_SRC_UART   = 4U,   /**< UART stream (ASCII or binary)             */
    ANALOG_SRC_INJECT = 5U,   /**< Software injection (test / simulation)    */
} AnalogSource_t;

/* =========================================================================
 * Channel descriptor (static, compile-time)
 * ========================================================================= */
typedef struct {
    uint8_t         id;            /**< Sensor ID — matches rule engine table  */
    const char     *name;
    AnalogSource_t  source;

    /* Hardware address — interpretation depends on source:
     *   ADC:   channel number (0-based)
     *   I2C:   device address (7-bit)
     *   SPI:   chip-select pin
     *   RS485: Modbus register address
     *   UART:  field index in incoming frame
     *   INJECT: ignored */
    uint32_t        hw_addr;

    /* Scaling: raw_to_scaled(raw) = raw * scale_num / scale_den + offset_x100 */
    int32_t         scale_num;     /**< Numerator of scale factor              */
    int32_t         scale_den;     /**< Denominator (must be != 0)             */
    int32_t         offset_x100;   /**< Offset already × 100                  */

    /* Thresholds (scaled × 100, matching rule engine convention).  0 = disabled. */
    int32_t         lo_thresh_x100;
    int32_t         hi_thresh_x100;

    uint32_t        sample_ms;     /**< Sampling period in ms                 */
    bool            enabled;
} AnalogChannelDesc_t;

/* Channel runtime state */
typedef struct {
    int32_t  last_value_x100;
    uint32_t last_sample_ms;
    uint32_t sample_count;
    uint32_t error_count;
    bool     threshold_hi_active;
    bool     threshold_lo_active;
    bool     fault;
} AnalogChannelState_t;

/* =========================================================================
 * ANALOG_CHANNEL_DEF — declare a static channel entry
 *
 * Example:
 *   ANALOG_CHANNEL_DEF(g_boiler_ch,
 *       1,               // sensor ID
 *       "BoilerTemp",
 *       ANALOG_SRC_ADC,
 *       0,               // ADC channel 0
 *       330, 4095,       // scale: (raw * 330) / 4095 gives °C x 100 range for NTC
 *       0,               // no offset
 *       1500,            // lo threshold: 15.00 °C
 *       8000,            // hi threshold: 80.00 °C
 *       100U             // 100 ms sample period
 *   );
 * ========================================================================= */
#define ANALOG_CHANNEL_DEF(var, id_, name_, src_, hw_addr_, \
                           scale_num_, scale_den_, offset_, \
                           lo_, hi_, period_ms_)            \
    static AnalogChannelDesc_t var = {                      \
        (id_), (name_), (src_), (hw_addr_),                 \
        (scale_num_), (scale_den_), (offset_),              \
        (lo_), (hi_), (period_ms_), true                    \
    }

/* =========================================================================
 * Manager API
 * ========================================================================= */

/** @brief Initialise manager. Must be called before Scheduler_AddTask. */
void AnalogSensorMgr_Init(void);

/**
 * @brief Register a channel with the manager.
 * @param desc  Pointer to a static AnalogChannelDesc_t.
 * @param state Pointer to a static AnalogChannelState_t (zeroed by Init).
 */
bool AnalogSensorMgr_AddChannel(AnalogChannelDesc_t *desc, AnalogChannelState_t *state);

/**
 * @brief Software injection — set a value for ANALOG_SRC_INJECT channels.
 * Also works for simulation of any source type.
 * @param sensor_id  Channel sensor_id.
 * @param value_x100 Scaled value (actual × 100).
 */
void AnalogSensorMgr_Inject(uint8_t sensor_id, int32_t value_x100);

/** @brief Scheduler task function. Register with Scheduler_AddTask. */
void AnalogSensorMgr_Task(void *arg);

/** @brief Get last known value for a channel (returns false if id not found). */
bool AnalogSensorMgr_GetValue(uint8_t sensor_id, int32_t *value_x100_out);

/** @brief Enable or disable a channel at runtime. */
void AnalogSensorMgr_SetEnabled(uint8_t sensor_id, bool enabled);

#endif /* ANALOG_SENSOR_MGR_H */
