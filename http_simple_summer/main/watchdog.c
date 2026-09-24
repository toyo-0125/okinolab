#include "watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdbool.h>

#include "cpu_usage.h"
#include "temprature.h"
#include "http_s.h"

static const char *TAG = "watchdog";

static void monitor_task(void *arg){

    while (1) {
        if (http_server_is_running()) {
            ESP_LOGI(TAG, "HTTP server: running");
        } else {
            ESP_LOGW(TAG, "HTTP server: stopped");
        }

        float total = cpu_usage_get_total();
        float core0 = cpu_usage_get_core0();
        float core1 = cpu_usage_get_core1();
        float temprature = get_cpu_temprature();

        ESP_LOGI(TAG,
                 "CPU total: %.1f%%, core0: %.1f%%, core1: %.1f%%, temperature: %.1f°C",
                 total, core0, core1, temprature);
            
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