#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"


typedef enum {
    APP_WIFI_STATUS_DISABLED = 0,
    APP_WIFI_STATUS_CONNECTING,
    APP_WIFI_STATUS_CONNECTED,
    APP_WIFI_STATUS_FAILED,
} app_wifi_status_t;


/* Start Wi-Fi Station mode without blocking the sensor tasks. */
esp_err_t app_wifi_start(void);

app_wifi_status_t app_wifi_get_status(void);

bool app_wifi_is_connected(void);

/* Intended for cloud_task, never for sensor_task. */
bool app_wifi_wait_connected(TickType_t timeout_ticks);

/* Copies the current IPv4 text into output. Returns false when offline. */
bool app_wifi_get_ip(char *output, size_t output_size);

#endif
