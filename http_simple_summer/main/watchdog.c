#include "watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "watchdog";

static void monitor_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "monitor running");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void watchdog_start(void)
{
    xTaskCreate(
        monitor_task,
        "system_monitor",
        4096,
        NULL,
        5,
        NULL
    );
}