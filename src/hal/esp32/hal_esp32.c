/**
 * @file hal_esp32.c
 * @brief ESP32 ESP-IDF HAL implementation for Rule Engine
 *
 * Include this file only when building for ESP32_PLATFORM.
 * Uses ESP-IDF driver APIs: gpio, adc_oneshot, i2c, rtc.
 */

#ifdef ESP32_PLATFORM

#include "re_hal.h"
#include "re_logger.h"
#include "config.h"

/* ESP-IDF headers */
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/i2c.h"
#include "esp_sntp.h"

#include <string.h>
#include <time.h>

/* =========================================================================
 * Log ring buffer (NVS / SPIFFS optional — RAM for now)
 * ========================================================================= */
#define ESP32_RAM_LOG_SIZE (256U)
static RE_LogEntry_t s_ram_log[ESP32_RAM_LOG_SIZE];
static uint16_t      s_ram_log_head  = 0U;
static uint16_t      s_ram_log_count = 0U;

/* ADC calibration */
static esp_adc_cal_characteristics_t s_adc_chars;

/* =========================================================================
 * HAL_RE_Init
 * ========================================================================= */
RE_Status_t HAL_RE_Init(void)
{
    /* Characterise ADC (using 11dB attenuation, 12-bit, Vref=1100mV) */
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12, 1100U, &s_adc_chars);

    RE_LOG_INFO("ESP32 Rule Engine HAL initialised");
    return RE_OK;
}

void HAL_RE_DeInit(void)
{
    RE_LOG_INFO("ESP32 HAL deinitialised");
}

/* =========================================================================
 * HAL_ReadSensor
 * ========================================================================= */
RE_Status_t HAL_ReadSensor(uint8_t sensor_id, SensorValue_t *value)
{
    if ((sensor_id == 0U) || (value == NULL))
    {
        return RE_ERR_INVALID_ID;
    }

    switch (RE_SensorGetInterface(sensor_id))
    {
        case RE_SENSOR_IF_ADC:
        {
            int      channel = RE_SensorGetADCChannel(sensor_id);
            uint32_t voltage = 0U;
            uint32_t raw;

            /* Multi-sample for noise reduction */
            uint8_t s;
            for (s = 0U; s < 4U; s++)
            {
                raw      = (uint32_t)adc1_get_raw(channel);
                voltage += esp_adc_cal_raw_to_voltage(raw, &s_adc_chars);
            }
            voltage /= 4U;

            /* Apply sensor-specific scaling from config */
            *value = (SensorValue_t)((voltage * RE_SensorGetScale(sensor_id)) / 3300U);
            break;
        }

        case RE_SENSOR_IF_GPIO:
        {
            gpio_num_t pin = (gpio_num_t)RE_SensorGetGPIOPin(sensor_id);
            int        lv  = gpio_get_level(pin);
            *value = (SensorValue_t)(lv ? 100 : 0);
            break;
        }

        case RE_SENSOR_IF_I2C:
        {
            /* Dispatch to sensor-specific I2C driver */
            /* Example: SHT30, BME280, etc. */
            /* i2c_read_bytes(..., value); */
            *value = 0;
            break;
        }

        default:
            return RE_ERR_INVALID_PARAM;
    }

    return RE_OK;
}

/* =========================================================================
 * HAL_WriteOutput
 * ========================================================================= */
RE_Status_t HAL_WriteOutput(uint8_t output_id, uint8_t state)
{
    gpio_num_t pin;

    if ((output_id == 0U) || (output_id > RE_MAX_OUTPUTS))
    {
        return RE_ERR_INVALID_ID;
    }

    pin = (gpio_num_t)RE_OutputGetGPIOPin(output_id);

    if (gpio_set_level(pin, state) != ESP_OK)
    {
        return RE_ERR_HAL;
    }

    return RE_OK;
}

/* =========================================================================
 * HAL_GetRTC — uses SNTP-synchronised system time
 * ========================================================================= */
RE_Status_t HAL_GetRTC(RE_RTC_t *rtc)
{
    time_t     now;
    struct tm  t;

    if (rtc == NULL)
    {
        return RE_ERR_NULL_PTR;
    }

    now = time(NULL);
    localtime_r(&now, &t);

    rtc->hour    = (uint8_t)t.tm_hour;
    rtc->minute  = (uint8_t)t.tm_min;
    rtc->second  = (uint8_t)t.tm_sec;
    rtc->day     = (uint8_t)t.tm_mday;
    rtc->month   = (uint8_t)(t.tm_mon + 1);
    rtc->year    = (uint16_t)(t.tm_year + 1900);
    rtc->weekday = (uint8_t)t.tm_wday;

    return RE_OK;
}

/* =========================================================================
 * HAL_GetTimestamp
 * ========================================================================= */
RE_Timestamp_t HAL_GetTimestamp(void)
{
    return (RE_Timestamp_t)(esp_timer_get_time() / 1000000ULL);
}

/* =========================================================================
 * HAL_GetMicros
 * ========================================================================= */
uint32_t HAL_GetMicros(void)
{
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFFUL);
}

/* =========================================================================
 * HAL_LogEvent
 * ========================================================================= */
RE_Status_t HAL_LogEvent(const RE_LogEntry_t *entry)
{
    if (entry == NULL)
    {
        return RE_ERR_NULL_PTR;
    }

    taskENTER_CRITICAL_ISR(NULL);
    s_ram_log[s_ram_log_head] = *entry;
    s_ram_log_head            = (uint16_t)((s_ram_log_head + 1U) % ESP32_RAM_LOG_SIZE);
    if (s_ram_log_count < ESP32_RAM_LOG_SIZE)
    {
        s_ram_log_count++;
    }
    taskEXIT_CRITICAL_ISR(NULL);

    /* Optional: write to NVS or SD via SPIFFS */

    return RE_OK;
}

RE_Status_t HAL_ReadLog(RE_LogEntry_t *buf, uint16_t count, uint16_t *read)
{
    uint16_t to_read;
    uint16_t i;
    uint16_t start;

    if ((buf == NULL) || (read == NULL))
    {
        return RE_ERR_NULL_PTR;
    }

    to_read = (count < s_ram_log_count) ? count : s_ram_log_count;
    *read   = to_read;
    start   = (s_ram_log_head >= to_read)
                  ? (uint16_t)(s_ram_log_head - to_read)
                  : (uint16_t)(ESP32_RAM_LOG_SIZE - (to_read - s_ram_log_head));

    for (i = 0U; i < to_read; i++)
    {
        buf[i] = s_ram_log[(start + i) % ESP32_RAM_LOG_SIZE];
    }

    return RE_OK;
}

void HAL_ClearLog(void)
{
    taskENTER_CRITICAL_ISR(NULL);
    s_ram_log_head  = 0U;
    s_ram_log_count = 0U;
    taskEXIT_CRITICAL_ISR(NULL);
}

/* =========================================================================
 * Watchdog
 * ========================================================================= */
#include "esp_task_wdt.h"

void HAL_KickWatchdog(void)
{
    esp_task_wdt_reset();
}

/* =========================================================================
 * Critical Section — FreeRTOS portMUX on ESP32
 * ========================================================================= */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void HAL_EnterCritical(void)
{
    taskENTER_CRITICAL(&s_mux);
}

void HAL_ExitCritical(void)
{
    taskEXIT_CRITICAL(&s_mux);
}

#endif /* ESP32_PLATFORM */
