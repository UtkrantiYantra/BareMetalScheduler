/**
 * @file main_esp32.c
 * @brief ESP32 ESP-IDF Rule Engine application entry point
 *
 * Place in platform/esp32/main/
 * Configure CMakeLists.txt to include src/core, src/hal/esp32, generated/.
 */

#ifdef ESP32_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "re_engine.h"
#include "re_hal.h"
#include "config.h"

static const char *TAG = "RE_APP";

/* =========================================================================
 * Rule engine task
 * ========================================================================= */
#define RE_TICK_PERIOD_MS  (100U)
#define RE_TASK_STACK_KB   (8192U)
#define RE_TASK_PRIO       (5U)

static void rule_engine_task(void *arg)
{
    RE_Status_t ret;
    TickType_t  last_wake;

    (void)arg;

    ret = RE_Init();
    if (ret != RE_OK)
    {
        ESP_LOGE(TAG, "RE_Init failed: %d", (int)ret);
        vTaskDelete(NULL);
        return;
    }

    last_wake = xTaskGetTickCount();

    for (;;)
    {
        (void)RE_Tick();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RE_TICK_PERIOD_MS));
    }
}

/* =========================================================================
 * Diagnostics task
 * ========================================================================= */
static void diagnostics_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(30000U));  /* Every 30 seconds */
        RE_PrintDiagnostics();
    }
}

/* =========================================================================
 * app_main — ESP-IDF entry point
 * ========================================================================= */
void app_main(void)
{
    ESP_LOGI(TAG, "Rule Engine v%u.%u.%u starting",
             RE_FRAMEWORK_VERSION_MAJOR,
             RE_FRAMEWORK_VERSION_MINOR,
             RE_FRAMEWORK_VERSION_PATCH);

    /* NVS init (needed for SNTP / WiFi if used) */
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    xTaskCreate(rule_engine_task,
                "rule_engine",
                RE_TASK_STACK_KB,
                NULL,
                RE_TASK_PRIO,
                NULL);

    xTaskCreate(diagnostics_task,
                "diagnostics",
                4096U,
                NULL,
                2U,
                NULL);
}

#endif /* ESP32_PLATFORM */
