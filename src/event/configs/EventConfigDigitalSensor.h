/**
 * @file EventConfigDigitalSensor.h
 * @brief Events published by DigitalSensorManager
 *
 * @EVENT_MODULE: DigitalSensor
 * @BASE_ID: 0x0500
 */

#ifndef EVENT_CONFIG_DIGITAL_SENSOR_H
#define EVENT_CONFIG_DIGITAL_SENSOR_H



/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_DIGITAL_RISING_EDGE                  0x0500U
#define EVT_DIGITAL_FALLING_EDGE                 0x0501U
#define EVT_DIGITAL_STATE_ON                     0x0502U
#define EVT_DIGITAL_STATE_OFF                    0x0503U
#define EVT_DIGITAL_DEBOUNCE_DONE                0x0504U
#define EVT_DIGITAL_FAULT                        0x0505U

typedef struct {
    uint8_t  sensor_id;
    uint8_t  state;          /**< 0=LOW 1=HIGH */
    uint32_t debounce_ms;    /**< Debounce window applied                 */
    uint32_t timestamp_ms;
} DigitalEdgePayload_t;

#endif /* EVENT_CONFIG_DIGITAL_SENSOR_H */
