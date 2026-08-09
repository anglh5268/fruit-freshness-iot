#ifndef CLOUD_PROTOCOL_H
#define CLOUD_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "sensor_task.h"


#define CLOUD_PROTOCOL_VERSION 1
#define CLOUD_REQUEST_BUFFER_SIZE 2048
#define CLOUD_RESPONSE_BUFFER_SIZE 2048


typedef struct {
    bool ok;
    char request_id[48];
    char risk_level[24];
    char summary[128];
    char advice[192];
    char model_source[32];
} cloud_report_t;


esp_err_t cloud_protocol_build_request(
    const sensor_snapshot_t *snapshot,
    const char *device_id,
    char *output,
    size_t output_size
);

esp_err_t cloud_protocol_parse_response(
    const char *json_text,
    cloud_report_t *report
);

esp_err_t cloud_protocol_build_deepseek_request(
    const sensor_snapshot_t *snapshot,
    const char *model,
    char *output,
    size_t output_size
);

esp_err_t cloud_protocol_parse_deepseek_response(
    const char *json_text,
    uint32_t sample_number,
    cloud_report_t *report
);

#endif
