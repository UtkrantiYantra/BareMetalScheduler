/**
 * @file sensor_driver.c
 * @brief Sensor driver registry implementation
 */

#include "sensor_driver.h"
#include <string.h>
#include <stddef.h>

#ifndef SENSOR_DRIVER_MAX
#define SENSOR_DRIVER_MAX 32U
#endif

static struct {
    SensorDriver_t *drvs[SENSOR_DRIVER_MAX];
    uint8_t         count;
} s_sd;

void SensorDriver_InitAll(void)
{
    uint8_t i;
    for (i = 0U; i < s_sd.count; i++) {
        SensorDriver_t *d = s_sd.drvs[i];
        if (d->ops && d->ops->init) {
            d->ready = d->ops->init(d->cfg);
        } else {
            d->ready = true;
        }
    }
}

bool SensorDriver_Register(SensorDriver_t *drv)
{
    if (!drv || !drv->ops || s_sd.count >= SENSOR_DRIVER_MAX) return false;
    s_sd.drvs[s_sd.count++] = drv;
    return true;
}

SensorDriver_t *SensorDriver_Find(uint8_t id)
{
    uint8_t i;
    for (i = 0U; i < s_sd.count; i++)
        if (s_sd.drvs[i]->id == id) return s_sd.drvs[i];
    return NULL;
}

bool SensorDriver_Read(uint8_t id, int32_t *raw_out)
{
    SensorDriver_t *d = SensorDriver_Find(id);
    if (!d || !d->ready || !d->ops->read) return false;
    if (d->ops->read(d->cfg, raw_out)) { d->read_ok++; return true; }
    d->read_err++;
    return false;
}
