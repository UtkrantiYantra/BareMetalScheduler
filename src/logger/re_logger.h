/**
 * @file re_logger.h
 * @brief Logging macros for the Rule Engine
 *
 * Maps to platform-appropriate output:
 *   Linux  → printf
 *   STM32  → UART / SWO
 *   ESP32  → ESP_LOG
 */

#ifndef RE_LOGGER_H
#define RE_LOGGER_H

#if defined(ESP32_PLATFORM)
    #include "esp_log.h"
    #define RE_TAG "RuleEngine"
    #define RE_LOG_INFO(fmt, ...)  ESP_LOGI(RE_TAG, fmt, ##__VA_ARGS__)
    #define RE_LOG_WARN(fmt, ...)  ESP_LOGW(RE_TAG, fmt, ##__VA_ARGS__)
    #define RE_LOG_ERROR(fmt, ...) ESP_LOGE(RE_TAG, fmt, ##__VA_ARGS__)
    #define RE_LOG_DEBUG(fmt, ...) ESP_LOGD(RE_TAG, fmt, ##__VA_ARGS__)
#elif defined(STM32_PLATFORM)
    /* Route to a platform UART print; define RE_UART_Printf in stm32 hal */
    extern void RE_UART_Printf(const char *fmt, ...);
    #define RE_LOG_INFO(fmt, ...)  RE_UART_Printf("[I] " fmt "\r\n", ##__VA_ARGS__)
    #define RE_LOG_WARN(fmt, ...)  RE_UART_Printf("[W] " fmt "\r\n", ##__VA_ARGS__)
    #define RE_LOG_ERROR(fmt, ...) RE_UART_Printf("[E] " fmt "\r\n", ##__VA_ARGS__)
    #define RE_LOG_DEBUG(fmt, ...) /* stripped for STM32 release */
#else
    /* Linux simulation */
    #include <stdio.h>
    #define RE_LOG_INFO(fmt, ...)  printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
    #define RE_LOG_WARN(fmt, ...)  printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
    #define RE_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define RE_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

#endif /* RE_LOGGER_H */
