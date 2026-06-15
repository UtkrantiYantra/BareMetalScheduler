/**
 * @file EventConfigAnalogSensor.h
 * @brief Events published by AnalogSensorManager
 *
 * HOW TO ADD EVENTS
 * -----------------
 * 1. Add a new EVT_ANALOG_* entry in the enum below.
 * 2. Run `make generate` — the build tool merges all EventConfig*.h files
 *    into generated/EventList.h automatically.
 * 3. Any module that includes EventList.h can subscribe to the new event.
 *
 * Convention: each module owns its own EventConfig file.
 * The generated EventList.h is the single source of truth for all IDs.
 */

#ifndef EVENT_CONFIG_ANALOG_SENSOR_H
#define EVENT_CONFIG_ANALOG_SENSOR_H

/* @EVENT_MODULE: AnalogSensor
 * @BASE_ID: 0x0100
 * Each event value must be unique across ALL EventConfig*.h files.
 * The build tool validates for collisions.
 */

/* Payload for EVT_ANALOG_READING_READY / THRESHOLD_HIGH / THRESHOLD_LOW */

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_ANALOG_READING_READY                 0x0100U
#define EVT_ANALOG_THRESHOLD_HIGH                0x0101U
#define EVT_ANALOG_THRESHOLD_LOW                 0x0102U
#define EVT_ANALOG_SENSOR_FAULT                  0x0103U
#define EVT_ANALOG_SENSOR_OK                     0x0104U
#define EVT_ANALOG_CALIBRATED                    0x0105U

typedef struct {
    uint8_t   sensor_id;       /**< Channel ID (matches re_engine sensor IDs)   */
    int32_t   value_x100;      /**< Scaled value: actual * 100                  */
    uint32_t  timestamp_ms;    /**< Scheduler tick at time of reading            */
    uint8_t   source;          /**< AnalogSource_t: ADC/I2C/SPI/RS485/UART      */
} AnalogReadingPayload_t;

/* Payload for EVT_ANALOG_SENSOR_FAULT */
typedef struct {
    uint8_t   sensor_id;
    uint8_t   error_code;
    uint32_t  timestamp_ms;
} AnalogFaultPayload_t;

#endif /* EVENT_CONFIG_ANALOG_SENSOR_H */
