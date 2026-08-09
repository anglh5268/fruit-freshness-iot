#include "cloud_task.h"

#include <string.h>

#include "app_wifi.h"
#include "cloud_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "material_classifier.h"
#include "sensor_task.h"


/*
 * HTTPS/TLS plus the JSON request and response buffers need substantially
 * more stack than the original PC-gateway HTTP path.  Keep enough headroom
 * for certificate verification and cJSON parsing when DeepSeek is selected.
 */
#define CLOUD_TASK_STACK_SIZE 16384
#define CLOUD_TASK_PRIORITY 4
#define CLOUD_POLL_PERIOD_MS 200
#define CLOUD_STABLE_SAMPLE_COUNT 10
#define CLOUD_REMOVAL_SAMPLE_COUNT 5
#define CLOUD_RETRY_DELAY_MS 5000


static const char *TAG = "CLOUD_TASK";
static bool s_task_started;
static SemaphoreHandle_t s_status_mutex;
static cloud_task_status_t s_status;


static void set_simple_state(cloud_task_state_t state)
{
    if (s_status_mutex == NULL ||
        xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    if (s_status.state != state) {
        s_status.state = state;
        s_status.revision++;
    }
    xSemaphoreGive(s_status_mutex);
}


static void set_analysis_state(const sensor_snapshot_t *snapshot)
{
    if (s_status_mutex == NULL ||
        xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    s_status.state = CLOUD_TASK_STATE_ANALYZING;
    s_status.report_valid = false;
    strlcpy(
        s_status.class_label,
        "nectarine",
        sizeof(s_status.class_label)
    );
    s_status.confidence_percent = (uint8_t)(
        snapshot->classification.confidence * 100.0f + 0.5f
    );
    strlcpy(
        s_status.freshness_state,
        freshness_classifier_label(&snapshot->freshness),
        sizeof(s_status.freshness_state)
    );
    s_status.freshness_confidence_percent = (uint8_t)(
        snapshot->freshness.confidence * 100.0f + 0.5f
    );
    strlcpy(
        s_status.freshness_risk_level,
        freshness_classifier_risk_level_text(snapshot->freshness.risk_level),
        sizeof(s_status.freshness_risk_level)
    );
    s_status.local_anomaly_detected =
        snapshot->freshness.local_anomaly_detected;
    s_status.freshness_positions =
        snapshot->freshness.completed_positions;
    s_status.distance_mm = snapshot->distance_mm;
    s_status.error_text[0] = '\0';
    s_status.revision++;
    xSemaphoreGive(s_status_mutex);
}


static void set_success_state(
    const sensor_snapshot_t *snapshot,
    const cloud_report_t *report
)
{
    if (s_status_mutex == NULL ||
        xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    s_status.state = CLOUD_TASK_STATE_SUCCESS;
    s_status.report_valid = true;
    strlcpy(
        s_status.class_label,
        "nectarine",
        sizeof(s_status.class_label)
    );
    s_status.confidence_percent = (uint8_t)(
        snapshot->classification.confidence * 100.0f + 0.5f
    );
    strlcpy(
        s_status.freshness_state,
        freshness_classifier_label(&snapshot->freshness),
        sizeof(s_status.freshness_state)
    );
    s_status.freshness_confidence_percent = (uint8_t)(
        snapshot->freshness.confidence * 100.0f + 0.5f
    );
    strlcpy(
        s_status.freshness_risk_level,
        freshness_classifier_risk_level_text(snapshot->freshness.risk_level),
        sizeof(s_status.freshness_risk_level)
    );
    s_status.local_anomaly_detected =
        snapshot->freshness.local_anomaly_detected;
    s_status.freshness_positions =
        snapshot->freshness.completed_positions;
    s_status.distance_mm = snapshot->distance_mm;
    s_status.report = *report;
    s_status.error_text[0] = '\0';
    s_status.revision++;
    xSemaphoreGive(s_status_mutex);
}


static void set_error_state(esp_err_t error)
{
    if (s_status_mutex == NULL ||
        xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    s_status.state = CLOUD_TASK_STATE_ERROR;
    s_status.report_valid = false;
    strlcpy(s_status.error_text, esp_err_to_name(error), sizeof(s_status.error_text));
    s_status.revision++;
    xSemaphoreGive(s_status_mutex);
}


static bool snapshot_is_eligible(const sensor_snapshot_t *snapshot)
{
    return snapshot->sample_number > 0 &&
           snapshot->distance_valid &&
           snapshot->distance_mm >= MATERIAL_CLASSIFIER_MIN_DISTANCE_MM &&
           snapshot->distance_mm <= MATERIAL_CLASSIFIER_MAX_DISTANCE_MM &&
           snapshot->spectrum_valid &&
           snapshot->freshness.result_valid &&
           snapshot->freshness.cloud_requested;
}


static bool report_contains_any(
    const char *text,
    const char *const phrases[],
    size_t phrase_count
)
{
    if (text == NULL || text[0] == '\0') {
        return true;
    }
    for (size_t index = 0; index < phrase_count; ++index) {
        if (strstr(text, phrases[index]) != NULL) {
            return true;
        }
    }
    return false;
}


static void enforce_report_consistency(
    const sensor_snapshot_t *snapshot,
    cloud_report_t *report
)
{
    static const char *const high_conflicts[] = {
        "状态良好",
        "未发现明显异常",
        "低风险",
        "风险较低",
        "可短期冷藏",
    };
    static const char *const medium_conflicts[] = {
        "状态良好",
        "未发现明显异常",
        "低风险",
        "高风险",
        "腐坏风险最高",
    };
    static const char *const low_conflicts[] = {
        "高风险",
        "中风险",
        "腐坏",
        "明显下降",
        "局部异常",
    };

    switch (snapshot->freshness.risk_level) {
        case FRESHNESS_RISK_HIGH:
            if (report_contains_any(
                    report->summary,
                    high_conflicts,
                    sizeof(high_conflicts) / sizeof(high_conflicts[0])
                )) {
                if (snapshot->freshness.local_anomaly_detected) {
                    snprintf(
                        report->summary,
                        sizeof(report->summary),
                        "四个检测面存在明显差异，P%u位置局部腐坏风险最高。",
                        (unsigned int)snapshot->freshness.max_risk_position
                    );
                } else {
                    strlcpy(
                        report->summary,
                        "四个检测面平均风险较高，新鲜度明显下降。",
                        sizeof(report->summary)
                    );
                }
            }
            strlcpy(
                report->advice,
                "高风险结果不建议继续存放或食用，请检查软烂、异味、"
                "渗液、霉斑，有异常时丢弃。",
                sizeof(report->advice)
            );
            break;

        case FRESHNESS_RISK_MEDIUM:
            if (report_contains_any(
                    report->summary,
                    medium_conflicts,
                    sizeof(medium_conflicts) / sizeof(medium_conflicts[0])
                )) {
                if (snapshot->freshness.local_anomaly_detected) {
                    snprintf(
                        report->summary,
                        sizeof(report->summary),
                        "四个检测面存在差异，P%u位置发现局部异常，风险中等。",
                        (unsigned int)snapshot->freshness.max_risk_position
                    );
                } else {
                    strlcpy(
                        report->summary,
                        "四个检测面平均风险升高，新鲜度存在下降趋势。",
                        sizeof(report->summary)
                    );
                }
            }
            strlcpy(
                report->advice,
                "建议尽快食用，并检查软烂、异味、渗液等异常，"
                "不建议继续存放。",
                sizeof(report->advice)
            );
            break;

        case FRESHNESS_RISK_LOW:
        default:
            if (report_contains_any(
                    report->summary,
                    low_conflicts,
                    sizeof(low_conflicts) / sizeof(low_conflicts[0])
                )) {
                strlcpy(
                    report->summary,
                    "四个检测面整体稳定，平均风险较低，未发现明显异常。",
                    sizeof(report->summary)
                );
            }
            strlcpy(
                report->advice,
                "当前风险较低，建议冷藏保存并优先食用，"
                "食用前结合外观触感确认。",
                sizeof(report->advice)
            );
            break;
    }
}


static void log_cloud_report(const sensor_snapshot_t *snapshot, const cloud_report_t *report)
{
    unsigned int confidence_percent =
        (unsigned int)(snapshot->classification.confidence * 100.0f + 0.5f);
    ESP_LOGI(
        TAG,
        "CLOUD_REPORT fruit=nectarine material_hint=%s confidence=%u freshness=%s "
        "freshness_confidence=%u risk=%s local=%u source=%s",
        snapshot->classification.label,
        confidence_percent,
        freshness_classifier_label(&snapshot->freshness),
        (unsigned int)(snapshot->freshness.confidence * 100.0f + 0.5f),
        report->risk_level,
        snapshot->freshness.local_anomaly_detected ? 1U : 0U,
        report->model_source
    );
    ESP_LOGI(TAG, "CLOUD_SUMMARY %s", report->summary);
    ESP_LOGI(TAG, "CLOUD_ADVICE %s", report->advice);
}


static void cloud_worker_task(void *argument)
{
    (void)argument;

    uint32_t last_sample_number = 0;
    char stable_label[32] = {0};
    unsigned int stable_count = 0;
    unsigned int removal_count = 0;
    bool uploaded_for_detection = false;

    if (!cloud_client_is_configured()) {
        set_simple_state(CLOUD_TASK_STATE_DISABLED);
        ESP_LOGW(
            TAG,
            "Cloud upload disabled: configure main/cloud_gateway_config.h"
        );
    } else {
        set_simple_state(CLOUD_TASK_STATE_WAITING_WIFI);
        ESP_LOGI(
            TAG,
            "Cloud task ready; waiting for Wi-Fi and %d stable samples",
            CLOUD_STABLE_SAMPLE_COUNT
        );
    }

    while (1) {
        if (!cloud_client_is_configured() || !app_wifi_is_connected()) {
            set_simple_state(
                cloud_client_is_configured()
                    ? CLOUD_TASK_STATE_WAITING_WIFI
                    : CLOUD_TASK_STATE_DISABLED
            );
            stable_count = 0;
            removal_count = 0;
            stable_label[0] = '\0';
            uploaded_for_detection = false;
            vTaskDelay(pdMS_TO_TICKS(CLOUD_POLL_PERIOD_MS));
            continue;
        }

        sensor_snapshot_t snapshot;
        if (!sensor_task_get_latest(&snapshot) ||
            snapshot.sample_number == last_sample_number) {
            vTaskDelay(pdMS_TO_TICKS(CLOUD_POLL_PERIOD_MS));
            continue;
        }
        last_sample_number = snapshot.sample_number;

        if (!uploaded_for_detection) {
            set_simple_state(CLOUD_TASK_STATE_READY);
        }

        if (!snapshot_is_eligible(&snapshot)) {
            stable_count = 0;
            stable_label[0] = '\0';

            if (uploaded_for_detection) {
                if (removal_count < CLOUD_REMOVAL_SAMPLE_COUNT) {
                    ++removal_count;
                }
                if (removal_count >= CLOUD_REMOVAL_SAMPLE_COUNT) {
                    uploaded_for_detection = false;
                    removal_count = 0;
                    set_simple_state(CLOUD_TASK_STATE_READY);
                    ESP_LOGI(TAG, "Target removed; cloud analysis re-armed");
                }
            } else {
                removal_count = 0;
                set_simple_state(CLOUD_TASK_STATE_READY);
            }
            vTaskDelay(pdMS_TO_TICKS(CLOUD_POLL_PERIOD_MS));
            continue;
        }

        removal_count = 0;
        if (uploaded_for_detection) {
            vTaskDelay(pdMS_TO_TICKS(CLOUD_POLL_PERIOD_MS));
            continue;
        }

        const char *freshness_label =
            freshness_classifier_label(&snapshot.freshness);
        if (strcmp(stable_label, freshness_label) == 0) {
            if (stable_count < CLOUD_STABLE_SAMPLE_COUNT) {
                ++stable_count;
            }
        } else {
            strlcpy(stable_label, freshness_label, sizeof(stable_label));
            stable_count = 1;
        }

        if (stable_count >= CLOUD_STABLE_SAMPLE_COUNT && !uploaded_for_detection) {
            ESP_LOGI(
                TAG,
                "Stable nectarine freshness result: state=%s confidence=%.2f "
                "distance=%u mm",
                freshness_label,
                snapshot.freshness.confidence,
                snapshot.distance_mm
            );

            set_analysis_state(&snapshot);
            cloud_report_t report;
            esp_err_t error = cloud_client_analyze(&snapshot, &report);
            if (error == ESP_OK) {
                strlcpy(
                    report.risk_level,
                    freshness_classifier_risk_level_text(
                        snapshot.freshness.risk_level
                    ),
                    sizeof(report.risk_level)
                );
                enforce_report_consistency(&snapshot, &report);
                uploaded_for_detection = true;
                set_success_state(&snapshot, &report);
                log_cloud_report(&snapshot, &report);
            } else {
                set_error_state(error);
                ESP_LOGW(
                    TAG,
                    "Cloud analysis failed; retrying in %d ms",
                    CLOUD_RETRY_DELAY_MS
                );
                stable_count = 0;
                vTaskDelay(pdMS_TO_TICKS(CLOUD_RETRY_DELAY_MS));
                continue;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CLOUD_POLL_PERIOD_MS));
    }
}


esp_err_t cloud_task_start(void)
{
    if (s_task_started) {
        return ESP_OK;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = CLOUD_TASK_STATE_DISABLED;
    s_status.revision = 1;
    s_status_mutex = xSemaphoreCreateMutex();
    if (s_status_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t result = xTaskCreate(
        cloud_worker_task,
        "cloud_worker",
        CLOUD_TASK_STACK_SIZE,
        NULL,
        CLOUD_TASK_PRIORITY,
        NULL
    );
    if (result != pdPASS) {
        vSemaphoreDelete(s_status_mutex);
        s_status_mutex = NULL;
        ESP_LOGE(TAG, "Failed to create cloud task");
        return ESP_ERR_NO_MEM;
    }

    s_task_started = true;
    return ESP_OK;
}


bool cloud_task_get_status(cloud_task_status_t *status)
{
    if (status == NULL || s_status_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }
    *status = s_status;
    xSemaphoreGive(s_status_mutex);
    return true;
}
