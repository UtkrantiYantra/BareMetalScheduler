/**
 * @file sensor_driver.h
 * @brief Sensor Driver Abstraction — uniform ops table for any sensor backend
 *
 * Decouples sensor managers from transport details.  A driver is a const
 * ops table + an instance config.  AnalogSensorManager's five source types
 * (ADC/I2C/SPI/RS485/UART) are adapted through this interface, and new
 * sensor families (1-Wire, CAN, virtual/computed) plug in without touching
 * manager code.
 *
 *   static const SensorDriverOps_t k_ads1115_ops = {
 *       .init   = ads1115_init,
 *       .read   = ads1115_read,
 *       .deinit = ads1115_deinit,
 *   };
 *   SENSOR_DRIVER_DEF(g_ads, 10, "ads1115", &k_ads1115_ops, &my_cfg);
 *   SensorDriver_Register(&g_ads);
 *   ...
 *   int32_t raw;
 *   SensorDriver_Read(10, &raw);
 */

#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool (*init)  (void *cfg);
    bool (*read)  (void *cfg, int32_t *raw_out);
    void (*deinit)(void *cfg);
} SensorDriverOps_t;

typedef struct {
    uint8_t                  id;
    const char              *name;
    const SensorDriverOps_t *ops;
    void                    *cfg;       /**< Driver-private config/state      */
    bool                     ready;
    uint32_t                 read_ok;
    uint32_t                 read_err;
} SensorDriver_t;

#define SENSOR_DRIVER_DEF(var, id_, name_, ops_, cfg_)  \
    static SensorDriver_t var = {                        \
        (id_), (name_), (ops_), (cfg_), false, 0U, 0U   \
    }

void            SensorDriver_InitAll(void);
bool            SensorDriver_Register(SensorDriver_t *drv);
SensorDriver_t *SensorDriver_Find(uint8_t id);
bool            SensorDriver_Read(uint8_t id, int32_t *raw_out);

#endif /* SENSOR_DRIVER_H */
