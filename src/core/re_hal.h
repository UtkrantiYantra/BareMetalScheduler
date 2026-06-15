/**
 * @file re_hal.h
 * @brief Hardware Abstraction Layer interface for Rule Engine
 *
 * All platform-specific code is isolated behind these interfaces.
 * Provide separate implementations for STM32, ESP32, and Linux.
 */

#ifndef RE_HAL_H
#define RE_HAL_H

#include "re_types.h"

/* =========================================================================
 * HAL Init / Deinit
 * ========================================================================= */

/**
 * @brief Initialise HAL layer (GPIOs, ADC, RTC, UART for log, etc.)
 * @return RE_OK on success
 */
RE_Status_t HAL_RE_Init(void);

/**
 * @brief De-initialise HAL layer (shutdown sequence)
 */
void HAL_RE_DeInit(void);

/* =========================================================================
 * Sensor HAL
 * ========================================================================= */

/**
 * @brief Read a sensor value from hardware.
 *
 * The HAL maps sensor_id to the physical peripheral (ADC channel, GPIO pin,
 * I2C device address, etc.) via the generated config tables.
 *
 * @param[in]  sensor_id  Unique sensor ID (1–99)
 * @param[out] value      Pointer to receive the scaled value (x100)
 * @return RE_OK on success, RE_ERR_HAL on hardware failure
 */
RE_Status_t HAL_ReadSensor(uint8_t sensor_id, SensorValue_t *value);

/* =========================================================================
 * Output HAL
 * ========================================================================= */

/**
 * @brief Write an output state to hardware.
 *
 * @param[in] output_id  Unique output ID (1–99)
 * @param[in] state      Desired state: 0=OFF, 1=ON
 * @return RE_OK on success
 */
RE_Status_t HAL_WriteOutput(uint8_t output_id, uint8_t state);

/* =========================================================================
 * RTC HAL
 * ========================================================================= */

/**
 * @brief Read current real-time clock value.
 *
 * @param[out] rtc  Pointer to RE_RTC_t structure to populate
 * @return RE_OK on success
 */
RE_Status_t HAL_GetRTC(RE_RTC_t *rtc);

/**
 * @brief Get monotonic timestamp (seconds since boot / epoch).
 *
 * Used for delay timers and log timestamps.
 *
 * @return Current timestamp in seconds
 */
RE_Timestamp_t HAL_GetTimestamp(void);

/**
 * @brief Get high-resolution microsecond counter for profiling.
 * @return Microseconds (wraps every ~71 minutes for uint32)
 */
uint32_t HAL_GetMicros(void);

/* =========================================================================
 * Logging / Storage HAL
 * ========================================================================= */

/**
 * @brief Persist a log entry to the storage backend.
 *
 * Backend may be RAM ring buffer, Flash, SD card, or UART — determined
 * at compile time by platform flags.
 *
 * @param[in] entry  Pointer to log entry
 * @return RE_OK on success
 */
RE_Status_t HAL_LogEvent(const RE_LogEntry_t *entry);

/**
 * @brief Read log entries from storage into caller buffer.
 *
 * @param[out] buf    Buffer to write entries into
 * @param[in]  count  Maximum entries to read
 * @param[out] read   Actual entries read
 * @return RE_OK on success
 */
RE_Status_t HAL_ReadLog(RE_LogEntry_t *buf, uint16_t count, uint16_t *read);

/**
 * @brief Clear all stored log entries.
 */
void HAL_ClearLog(void);

/* =========================================================================
 * Watchdog HAL
 * ========================================================================= */

/**
 * @brief Kick/feed hardware watchdog.
 */
void HAL_KickWatchdog(void);

/* =========================================================================
 * Critical section HAL (for FreeRTOS portability)
 * ========================================================================= */

void HAL_EnterCritical(void);
void HAL_ExitCritical(void);

#endif /* RE_HAL_H */

#if defined(LINUX_PLATFORM)
/* Linux simulation helpers — declared here so all translation units can use them */
void RE_HAL_SimSetSensor(uint8_t sensor_id, int32_t value);
void RE_HAL_SimForceResample(void);
#endif
