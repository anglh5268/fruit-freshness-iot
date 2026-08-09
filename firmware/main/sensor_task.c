#include "sensor_task.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "vl53l0x.h"
#include "as7341.h"
#include "freshness_classifier.h"
#include "material_classifier.h"
#include "xl9555.h"


#define SENSOR_TASK_STACK_SIZE  8192
#define SENSOR_TASK_PRIORITY    5
#define SENSOR_PERIOD_MS        100
#define AS7341_LED_DRIVE         4


static const char *TAG = "SENSOR_TASK";

static SemaphoreHandle_t s_data_mutex = NULL;
static sensor_snapshot_t s_latest_data;
static sensor_snapshot_t s_completed_measurement;
static bool s_completed_measurement_valid = false;
static bool s_task_started = false;
static bool s_show_sensor_details = false;


static void sensor_acquisition_task(void *argument)
{
    (void)argument;

    TickType_t last_wake_time = xTaskGetTickCount();

    const TickType_t period =
        pdMS_TO_TICKS(SENSOR_PERIOD_MS);

    while (1) {
        sensor_snapshot_t new_data;
        memset(&new_data, 0, sizeof(new_data));

        esp_err_t distance_err =
            vl53l0x_read_distance(
                &new_data.distance_mm
            );

        new_data.distance_valid =
            (distance_err == ESP_OK);

        if (distance_err != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Distance read failed: %s",
                esp_err_to_name(distance_err)
            );
        }

        esp_err_t spectrum_err =
            as7341_read_reflectance(
                &new_data.ambient_spectrum,
                &new_data.illuminated_spectrum,
                &new_data.spectrum,
                AS7341_LED_DRIVE
            );

        new_data.spectrum_valid =
            (spectrum_err == ESP_OK);

        if (spectrum_err != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Reflectance read failed: %s",
                esp_err_to_name(spectrum_err)
            );
        }

        const bool distance_in_model_range =
            new_data.distance_valid &&
            new_data.distance_mm >= MATERIAL_CLASSIFIER_MIN_DISTANCE_MM &&
            new_data.distance_mm <= MATERIAL_CLASSIFIER_MAX_DISTANCE_MM;

        if (new_data.spectrum_valid && distance_in_model_range) {
            esp_err_t classification_err =
                material_classifier_predict(
                    &new_data.spectrum,
                    &new_data.classification
                );

            if (classification_err != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Classification failed: %s",
                    esp_err_to_name(classification_err)
                );
            }
        }

        const uint8_t key = xl9555_key_scan(0);
        freshness_command_t freshness_command = FRESHNESS_COMMAND_NONE;

        switch (key) {
            case KEY0_PRES:
                freshness_command = FRESHNESS_COMMAND_CONFIRM;
                break;
            case KEY1_PRES:
                freshness_command = FRESHNESS_COMMAND_RETRY;
                break;
            case KEY2_PRES:
                s_show_sensor_details = !s_show_sensor_details;
                ESP_LOGI(
                    TAG,
                    "Display page: %s",
                    s_show_sensor_details ? "sensor details" : "workflow"
                );
                break;
            case KEY3_PRES:
                freshness_command = FRESHNESS_COMMAND_RESET;
                break;
            default:
                break;
        }

        if (freshness_command != FRESHNESS_COMMAND_NONE) {
            esp_err_t command_error =
                freshness_classifier_handle_command(
                    freshness_command,
                    new_data.spectrum_valid && distance_in_model_range
                );
            if (command_error == ESP_OK) {
                ESP_LOGI(TAG, "Freshness key accepted: %u", key);
            } else {
                ESP_LOGI(
                    TAG,
                    "Freshness key ignored in current state: %u",
                    key
                );
            }
        }

        esp_err_t freshness_error = freshness_classifier_update(
            new_data.spectrum_valid && distance_in_model_range,
            new_data.spectrum_valid ? &new_data.spectrum : NULL,
            &new_data.freshness
        );
        if (freshness_error != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Freshness update failed: %s",
                esp_err_to_name(freshness_error)
            );
        }

        /*
         * P4完成时冻结最后一个有效测量快照。用户查看结果、移开水果后再按
         * KEY0请求AI时，云任务仍使用该快照，而不是使用移开后的无效距离。
         * freshness字段保持实时，以便cloud_requested按键状态可以继续更新。
         */
        if (new_data.freshness.result_valid) {
            if (!s_completed_measurement_valid) {
                s_completed_measurement = new_data;
                s_completed_measurement_valid = true;
            } else {
                new_data.distance_valid =
                    s_completed_measurement.distance_valid;
                new_data.distance_mm =
                    s_completed_measurement.distance_mm;
                new_data.spectrum_valid =
                    s_completed_measurement.spectrum_valid;
                new_data.spectrum =
                    s_completed_measurement.spectrum;
                new_data.ambient_spectrum =
                    s_completed_measurement.ambient_spectrum;
                new_data.illuminated_spectrum =
                    s_completed_measurement.illuminated_spectrum;
                new_data.classification =
                    s_completed_measurement.classification;
            }
        } else if (new_data.freshness.stage == FRESHNESS_STAGE_IDLE) {
            memset(
                &s_completed_measurement,
                0,
                sizeof(s_completed_measurement)
            );
            s_completed_measurement_valid = false;
        }
        new_data.ui_show_sensor_details = s_show_sensor_details;

        /*
         * 获取互斥锁后更新共享数据。
         * 显示任务不会读到更新一半的数据。
         */
        if (xSemaphoreTake(
                s_data_mutex,
                portMAX_DELAY) == pdTRUE) {

            new_data.sample_number =
                s_latest_data.sample_number + 1;

            s_latest_data = new_data;

            xSemaphoreGive(s_data_mutex);
        }

        /*
         * 使用vTaskDelayUntil，目标是每100ms开始一次采集。
         */
        vTaskDelayUntil(
            &last_wake_time,
            period
        );
    }
}


esp_err_t sensor_task_start(void)
{
    if (s_task_started) {
        return ESP_OK;
    }

    memset(
        &s_latest_data,
        0,
        sizeof(s_latest_data)
    );
    memset(
        &s_completed_measurement,
        0,
        sizeof(s_completed_measurement)
    );
    s_completed_measurement_valid = false;
    freshness_classifier_reset();
    s_show_sensor_details = false;

    s_data_mutex = xSemaphoreCreateMutex();

    if (s_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create data mutex");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t result = xTaskCreate(
        sensor_acquisition_task,
        "sensor_acquisition",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");

        vSemaphoreDelete(s_data_mutex);
        s_data_mutex = NULL;

        return ESP_ERR_NO_MEM;
    }

    s_task_started = true;

    ESP_LOGI(
        TAG,
        "Sensor task started, target period = %d ms",
        SENSOR_PERIOD_MS
    );

    return ESP_OK;
}


bool sensor_task_get_latest(
    sensor_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_data_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(
            s_data_mutex,
            pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }

    *snapshot = s_latest_data;

    xSemaphoreGive(s_data_mutex);

    return true;
}
