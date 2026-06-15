/**
 * @file digital_sensor_mgr.h
 * @brief Digital (binary) input sensor manager with debounce
 *
 * Publishes EVT_DIGITAL_RISING_EDGE / FALLING_EDGE / DEBOUNCE_DONE events.
 * Also writes the debounced state into the rule engine sensor table.
 */

#ifndef DIGITAL_SENSOR_MGR_H
#define DIGITAL_SENSOR_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

typedef struct {
    uint8_t       id;            /**< Matches rule engine sensor ID           */
    const char   *name;
    uint32_t      gpio_pin;
    uint32_t      debounce_ms;   /**< Stability window before state accepted   */
    bool          active_high;   /**< true = ON when GPIO reads high           */
    bool          enabled;
} DigitalSensorDesc_t;

typedef struct {
    uint8_t  stable_state;       /**< 0=OFF 1=ON — last debounced state       */
    uint8_t  raw_state;          /**< Raw GPIO read                           */
    uint32_t change_time_ms;     /**< When raw state last changed             */
    bool     debouncing;
    uint32_t edge_count;
} DigitalSensorState_t;

#define DIGITAL_SENSOR_DEF(var, id_, name_, pin_, deb_ms_, active_hi_) \
    static DigitalSensorDesc_t var = {                                  \
        (id_), (name_), (pin_), (deb_ms_), (active_hi_), true          \
    }

void DigitalSensorMgr_Init(void);
bool DigitalSensorMgr_AddSensor(DigitalSensorDesc_t *desc, DigitalSensorState_t *state);
uint8_t DigitalSensorMgr_GetState(uint8_t sensor_id);
void DigitalSensorMgr_Task(void *arg);

/* Simulation inject (Linux / test) */
void DigitalSensorMgr_Inject(uint8_t sensor_id, uint8_t state);

#endif /* DIGITAL_SENSOR_MGR_H */
