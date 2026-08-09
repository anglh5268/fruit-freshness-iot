#include "cloud_protocol.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_timer.h"


static bool copy_json_string(
    const cJSON *root,
    const char *name,
    char *output,
    size_t output_size
)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    strlcpy(output, item->valuestring, output_size);
    return true;
}


static bool add_spectrum(
    cJSON *measurement,
    const as7341_spectral_data_t *spectrum
)
{
    cJSON *json_spectrum = cJSON_AddObjectToObject(measurement, "spectrum");
    if (json_spectrum == NULL) {
        return false;
    }

    return cJSON_AddNumberToObject(json_spectrum, "F1_415nm", spectrum->f1_415nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F2_445nm", spectrum->f2_445nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F3_480nm", spectrum->f3_480nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F4_515nm", spectrum->f4_515nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F5_555nm", spectrum->f5_555nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F6_590nm", spectrum->f6_590nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F7_630nm", spectrum->f7_630nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "F8_680nm", spectrum->f8_680nm) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "Clear", spectrum->clear) != NULL &&
           cJSON_AddNumberToObject(json_spectrum, "NIR", spectrum->nir) != NULL;
}


esp_err_t cloud_protocol_build_request(
    const sensor_snapshot_t *snapshot,
    const char *device_id,
    char *output,
    size_t output_size
)
{
    if (snapshot == NULL || device_id == NULL || output == NULL ||
        output_size == 0 || !snapshot->distance_valid ||
        !snapshot->spectrum_valid || !snapshot->classification.valid ||
        !snapshot->freshness.result_valid) {
        return ESP_ERR_INVALID_ARG;
    }

    char request_id[48];
    snprintf(
        request_id,
        sizeof(request_id),
        "esp32-%lu",
        (unsigned long)snapshot->sample_number
    );

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *measurement = NULL;
    cJSON *edge_result = NULL;
    bool success =
        cJSON_AddNumberToObject(root, "protocol_version", CLOUD_PROTOCOL_VERSION) != NULL &&
        cJSON_AddStringToObject(root, "request_id", request_id) != NULL &&
        cJSON_AddStringToObject(root, "device_id", device_id) != NULL &&
        cJSON_AddNumberToObject(
            root,
            "timestamp_ms",
            (double)(esp_timer_get_time() / 1000)
        ) != NULL &&
        (measurement = cJSON_AddObjectToObject(root, "measurement")) != NULL &&
        cJSON_AddNumberToObject(
            measurement,
            "distance_mm",
            snapshot->distance_mm
        ) != NULL &&
        add_spectrum(measurement, &snapshot->spectrum) &&
        (edge_result = cJSON_AddObjectToObject(root, "edge_result")) != NULL &&
        cJSON_AddStringToObject(edge_result, "task", "fruit_freshness") != NULL &&
        cJSON_AddStringToObject(
            edge_result,
            "label",
            snapshot->classification.label
        ) != NULL &&
        cJSON_AddNumberToObject(
            edge_result,
            "confidence",
            snapshot->classification.confidence
        ) != NULL &&
        cJSON_AddStringToObject(
            edge_result,
            "freshness_state",
            freshness_classifier_label(&snapshot->freshness)
        ) != NULL &&
        cJSON_AddNumberToObject(
            edge_result,
            "freshness_confidence",
            snapshot->freshness.confidence
        ) != NULL &&
        cJSON_AddStringToObject(
            edge_result,
            "risk_level",
            freshness_classifier_risk_level_text(
                snapshot->freshness.risk_level
            )
        ) != NULL &&
        cJSON_AddNumberToObject(
            edge_result,
            "mean_risk_probability",
            snapshot->freshness.mean_risk_probability
        ) != NULL &&
        cJSON_AddNumberToObject(
            edge_result,
            "max_position_risk_probability",
            snapshot->freshness.max_position_risk_probability
        ) != NULL &&
        cJSON_AddNumberToObject(
            edge_result,
            "max_risk_position",
            snapshot->freshness.max_risk_position
        ) != NULL &&
        cJSON_AddBoolToObject(
            edge_result,
            "local_anomaly_detected",
            snapshot->freshness.local_anomaly_detected
        ) != NULL;

    if (!success) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_PrintPreallocated(root, output, (int)output_size, false)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    cJSON_Delete(root);
    return ESP_OK;
}


esp_err_t cloud_protocol_parse_response(
    const char *json_text,
    cloud_report_t *report
)
{
    if (json_text == NULL || report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "protocol_version");
    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
    bool valid = cJSON_IsNumber(version) &&
                 version->valueint == CLOUD_PROTOCOL_VERSION &&
                 cJSON_IsTrue(ok) &&
                 copy_json_string(
                     root, "request_id", report->request_id, sizeof(report->request_id)
                 ) &&
                 copy_json_string(
                     root, "risk_level", report->risk_level, sizeof(report->risk_level)
                 ) &&
                 copy_json_string(root, "summary", report->summary, sizeof(report->summary)) &&
                 copy_json_string(root, "advice", report->advice, sizeof(report->advice)) &&
                 copy_json_string(
                     root, "model_source", report->model_source, sizeof(report->model_source)
                 );

    cJSON_Delete(root);
    if (!valid) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    report->ok = true;
    return ESP_OK;
}


esp_err_t cloud_protocol_build_deepseek_request(
    const sensor_snapshot_t *snapshot,
    const char *model,
    char *output,
    size_t output_size
)
{
    if (snapshot == NULL || model == NULL || model[0] == '\0' ||
        output == NULL || output_size == 0 || !snapshot->distance_valid ||
        !snapshot->spectrum_valid || !snapshot->freshness.result_valid) {
        return ESP_ERR_INVALID_ARG;
    }

    static const char *system_prompt =
        "You write a useful on-device report for a nectarine spectral sensor. "
        "The edge result is authoritative: never contradict freshness_state, "
        "confidence, risk_level, mean_risk or max_position_risk, and never "
        "invent visible or odor evidence. Return JSON only, without Markdown: "
        "{\"risk_level\":\"low\",\"summary\":\"...\",\"advice\":\"...\"}. "
        "summary must be 15-30 Simplified Chinese characters and explain the "
        "four-surface result, especially local_anomaly and the highest-risk "
        "position. advice must be 15-36 Simplified Chinese characters and give "
        "a concrete storage, eating or inspection action. Advice policy is "
        "strict: low may recommend refrigerated storage; medium must recommend "
        "eating soon plus inspection; high must say not to continue storage or "
        "eating and must never say the fruit is in good condition. Do not give "
        "a generic disclaimer as the main answer. To fit the device font, only "
        "use Chinese "
        "characters from this set: "
        "分析建议结论新鲜度风险高低中油桃四个检测面整体稳定未发现"
        "明显异常存在差异局部腐坏概率较请检查软烂异味渗液霉斑尽快"
        "食用不要继续存放如有应丢弃当前状态良好可短期冷藏保存并优先"
        "结果显示下降依据最高位置平均综合判断模型仅供参考结合外观触感"
        "确认第处中等趋势升或前，。；：、. ASCII P and digits are allowed.";

    char user_prompt[512];
    int prompt_length = snprintf(
        user_prompt,
        sizeof(user_prompt),
        "Generate the report for task=nectarine_freshness; fruit=nectarine; "
        "distance_mm=%u; freshness_state=%s; "
        "freshness_confidence=%.2f; risk_level=%s; mean_risk=%.3f; "
        "max_position_risk=%.3f; max_risk_position=P%u; "
        "local_anomaly=%s; measured_positions=%u.",
        snapshot->distance_mm,
        freshness_classifier_label(&snapshot->freshness),
        snapshot->freshness.confidence,
        freshness_classifier_risk_level_text(
            snapshot->freshness.risk_level
        ),
        snapshot->freshness.mean_risk_probability,
        snapshot->freshness.max_position_risk_probability,
        snapshot->freshness.max_risk_position,
        snapshot->freshness.local_anomaly_detected ? "true" : "false",
        snapshot->freshness.completed_positions
    );
    if (prompt_length < 0 || (size_t)prompt_length >= sizeof(user_prompt)) {
        return ESP_ERR_INVALID_SIZE;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *thinking = cJSON_AddObjectToObject(root, "thinking");
    cJSON *response_format = cJSON_AddObjectToObject(root, "response_format");
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    bool success =
        cJSON_AddStringToObject(root, "model", model) != NULL &&
        cJSON_AddBoolToObject(root, "stream", false) != NULL &&
        cJSON_AddNumberToObject(root, "max_tokens", 180) != NULL &&
        thinking != NULL &&
        cJSON_AddStringToObject(thinking, "type", "disabled") != NULL &&
        response_format != NULL &&
        cJSON_AddStringToObject(response_format, "type", "json_object") != NULL &&
        messages != NULL;
    if (!success) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    cJSON *system_message = cJSON_CreateObject();
    if (system_message == NULL ||
        cJSON_AddStringToObject(system_message, "role", "system") == NULL ||
        cJSON_AddStringToObject(system_message, "content", system_prompt) == NULL ||
        !cJSON_AddItemToArray(messages, system_message)) {
        cJSON_Delete(system_message);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    cJSON *user_message = cJSON_CreateObject();
    if (user_message == NULL ||
        cJSON_AddStringToObject(user_message, "role", "user") == NULL ||
        cJSON_AddStringToObject(user_message, "content", user_prompt) == NULL ||
        !cJSON_AddItemToArray(messages, user_message)) {
        cJSON_Delete(user_message);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_PrintPreallocated(root, output, (int)output_size, false)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    cJSON_Delete(root);
    return ESP_OK;
}


esp_err_t cloud_protocol_parse_deepseek_response(
    const char *json_text,
    uint32_t sample_number,
    cloud_report_t *report
)
{
    if (json_text == NULL || report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    const cJSON *choice = cJSON_IsArray(choices)
        ? cJSON_GetArrayItem(choices, 0)
        : NULL;
    const cJSON *message = cJSON_IsObject(choice)
        ? cJSON_GetObjectItemCaseSensitive(choice, "message")
        : NULL;
    const cJSON *content = cJSON_IsObject(message)
        ? cJSON_GetObjectItemCaseSensitive(message, "content")
        : NULL;

    if (!cJSON_IsString(content) || content->valuestring == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *report_json = cJSON_Parse(content->valuestring);
    cJSON_Delete(root);
    if (report_json == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    bool valid =
        copy_json_string(
            report_json,
            "risk_level",
            report->risk_level,
            sizeof(report->risk_level)
        ) &&
        copy_json_string(
            report_json,
            "summary",
            report->summary,
            sizeof(report->summary)
        ) &&
        copy_json_string(
            report_json,
            "advice",
            report->advice,
            sizeof(report->advice)
        );
    cJSON_Delete(report_json);
    if (!valid) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    snprintf(
        report->request_id,
        sizeof(report->request_id),
        "esp32-%lu",
        (unsigned long)sample_number
    );
    strlcpy(report->model_source, "deepseek", sizeof(report->model_source));
    report->ok = true;
    return ESP_OK;
}
