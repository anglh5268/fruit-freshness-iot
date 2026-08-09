#ifndef CLOUD_TASK_H
#define CLOUD_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "cloud_protocol.h"
#include "esp_err.h"


typedef enum {
    CLOUD_TASK_STATE_DISABLED = 0,
    CLOUD_TASK_STATE_WAITING_WIFI,
    CLOUD_TASK_STATE_READY,
    CLOUD_TASK_STATE_ANALYZING,
    CLOUD_TASK_STATE_SUCCESS,
    CLOUD_TASK_STATE_ERROR,
} cloud_task_state_t;


typedef struct {
    cloud_task_state_t state;
    uint32_t revision;
    bool report_valid;
    char class_label[32];
    uint8_t confidence_percent;
    char freshness_state[16];
    uint8_t freshness_confidence_percent;
    char freshness_risk_level[16];
    bool local_anomaly_detected;
    uint8_t freshness_positions;
    uint16_t distance_mm;
    char error_text[64];
    cloud_report_t report;
} cloud_task_status_t;


esp_err_t cloud_task_start(void);

bool cloud_task_get_status(cloud_task_status_t *status);

#endif
