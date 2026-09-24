#include "cpu_usage.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "esp_timer.h"


/*
 * CPU使用率のサンプリング周期
 */
#define CPU_USAGE_SAMPLE_MS 1000


/*
 * CPU使用率
 *
 * monitor task が更新し、
 * main.c のHTTP handlerなどからgetter経由で読み取る。
 */
static volatile float s_cpu_total = 0.0f;
static volatile float s_cpu_core0 = 0.0f;
static volatile float s_cpu_core1 = 0.0f;


/*
 * 二重初期化防止
 */
static bool s_initialized = false;


static const char *TAG = "cpu_usage";


/*
 * 0～100%の範囲に制限する。
 */
static float clamp_usage(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}


/*
 * CPU使用率監視タスク
 *
 * ESP32には各CPUコア用のIDLEタスクが存在する。
 *
 *   IDLE0 → Core 0
 *   IDLE1 → Core 1
 *
 * 各IDLEタスクの累積実行時間を一定周期で取得し、
 * 前回値との差分からIDLE率を計算する。
 *
 * CPU使用率 = 100% - IDLE率
 */
static void cpu_usage_monitor_task(void *arg)
{
    /*
     * IDLEタスクのハンドルを取得
     *
     * IDLE0 → Core 0
     * IDLE1 → Core 1
     */
    TaskHandle_t idle0_handle = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1_handle = xTaskGetIdleTaskHandleForCore(1);

    /*
     * 前回測定時の累積実行時間
     */
    uint32_t previous_idle0 = 0;
    uint32_t previous_idle1 = 0;
    uint32_t previous_total_runtime = 0;

    bool first_sample = true;

    ESP_LOGI(TAG, "CPU usage monitor started");

    while (1) {

        /*
         * 現在存在するタスク数を取得
         */
        UBaseType_t task_count = uxTaskGetNumberOfTasks();

        /*
         * 全タスクの状態を保存する配列を確保
         */
        TaskStatus_t *task_status_array =
            pvPortMalloc(task_count * sizeof(TaskStatus_t));

        if (task_status_array == NULL) {

            ESP_LOGE(
                TAG,
                "Failed to allocate task status array"
            );

            vTaskDelay(
                pdMS_TO_TICKS(CPU_USAGE_SAMPLE_MS)
            );

            continue;
        }

        /*
         * 全タスクの状態と、
         * システム全体の累積実行時間を取得
         */
        uint32_t total_runtime = 0;

        UBaseType_t actual_task_count =
            uxTaskGetSystemState(
                task_status_array,
                task_count,
                &total_runtime
            );


        /*
         * IDLE0 / IDLE1 の累積実行時間を探す
         */
        uint32_t current_idle0 = 0;
        uint32_t current_idle1 = 0;

        bool found_idle0 = false;
        bool found_idle1 = false;

        for (UBaseType_t i = 0;
             i < actual_task_count;
             i++) {

            if (task_status_array[i].xHandle ==
                idle0_handle) {

                current_idle0 =
                    task_status_array[i]
                        .ulRunTimeCounter;

                found_idle0 = true;
            }

            if (task_status_array[i].xHandle ==
                idle1_handle) {

                current_idle1 =
                    task_status_array[i]
                        .ulRunTimeCounter;

                found_idle1 = true;
            }
        }


        /*
         * 配列はもう不要なので解放
         */
        vPortFree(task_status_array);


        /*
         * IDLEタスクが見つからなかった場合は
         * 今回の測定を破棄
         */
        if (!found_idle0 || !found_idle1) {

            ESP_LOGW(
                TAG,
                "Idle task not found"
            );

            vTaskDelay(
                pdMS_TO_TICKS(CPU_USAGE_SAMPLE_MS)
            );

            continue;
        }


        /*
         * 初回は比較対象がないので、
         * 基準値を保存するだけ
         */
        if (first_sample) {

            previous_idle0 = current_idle0;
            previous_idle1 = current_idle1;

            previous_total_runtime =
                total_runtime;

            first_sample = false;

        } else {

            /*
             * 前回から今回までの差分
             */
            uint32_t delta_idle0 =
                current_idle0 - previous_idle0;

            uint32_t delta_idle1 =
                current_idle1 - previous_idle1;

            uint32_t delta_total =
                total_runtime -
                previous_total_runtime;


            if (delta_total > 0) {

                /*
                 * 各コアのIDLE率を計算
                 */
                float idle0_percent =
                    ((float)delta_idle0 /
                     (float)delta_total)
                    * 100.0f;

                float idle1_percent =
                    ((float)delta_idle1 /
                     (float)delta_total)
                    * 100.0f;


                /*
                 * CPU使用率
                 *
                 * 100% - IDLE率
                 */
                float core0 =
                    100.0f - idle0_percent;

                float core1 =
                    100.0f - idle1_percent;


                core0 = clamp_usage(core0);
                core1 = clamp_usage(core1);


                /*
                 * 最新値を保存
                 */
                s_cpu_core0 = core0;
                s_cpu_core1 = core1;

                s_cpu_total =
                    (core0 + core1) / 2.0f;
            }


            /*
             * 今回値を次回の基準値にする
             */
            previous_idle0 = current_idle0;
            previous_idle1 = current_idle1;

            previous_total_runtime =
                total_runtime;
        }


        /*
         * 1秒待つ
         */
        vTaskDelay(
            pdMS_TO_TICKS(CPU_USAGE_SAMPLE_MS)
        );
    }
}


/*
 * CPU使用率監視開始
 */
void cpu_usage_init(void)
{
    /*
     * app_main()などから誤って2回呼ばれても、
     * monitor taskを二重生成しない。
     */
    if (s_initialized) {
        ESP_LOGW(
            TAG,
            "CPU usage monitor already initialized"
        );

        return;
    }


    BaseType_t result =
        xTaskCreate(
            cpu_usage_monitor_task,
            "cpu_usage_monitor",

            /*
             * このタスクは非常に軽いので
             * 大きなstackは不要。
             */
            3072,

            NULL,

            /*
             * 通常の監視処理なので
             * 高優先度にしない。
             */
            2,

            NULL
        );


    if (result == pdPASS) {

        s_initialized = true;

        ESP_LOGI(
            TAG,
            "CPU usage task created"
        );

    } else {

        ESP_LOGE(
            TAG,
            "Failed to create CPU usage task"
        );
    }
}


/*
 * 全体CPU使用率取得
 */
float cpu_usage_get_total(void)
{
    return s_cpu_total;
}


/*
 * Core 0 CPU使用率取得
 */
float cpu_usage_get_core0(void)
{
    return s_cpu_core0;
}


/*
 * Core 1 CPU使用率取得
 */
float cpu_usage_get_core1(void)
{
    return s_cpu_core1;
}