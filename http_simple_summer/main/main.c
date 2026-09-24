
#include <unistd.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_event.h"

//making point
#include "http_s.h"
#include "cpu_usage.h"


void app_main(void)
{
    cpu_usage_init();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    http_server_start();

    while (1) {
        sleep(5);
    }
}
