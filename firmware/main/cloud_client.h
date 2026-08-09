#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <stdbool.h>

#include "cloud_protocol.h"
#include "esp_err.h"
#include "sensor_task.h"


bool cloud_client_is_configured(void);

/* Returns the configured cloud provider name for status displays/logs. */
const char *cloud_client_provider_name(void);

esp_err_t cloud_client_analyze(
    const sensor_snapshot_t *snapshot,
    cloud_report_t *report
);

#endif
