#include "app_wifi.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"


#if APP_WIFI_LOCAL_CREDENTIALS
#include "app_wifi_credentials.h"
#define APP_WIFI_HAS_LOCAL_CREDENTIALS 1
#else
#define APP_WIFI_HAS_LOCAL_CREDENTIALS 0
#define APP_WIFI_SSID ""
#define APP_WIFI_PASSWORD ""
#endif


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_MAXIMUM_RETRY 8


static const char *TAG = "APP_WIFI";
static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t s_wifi_event_instance;
static esp_event_handler_instance_t s_ip_event_instance;
static app_wifi_status_t s_status = APP_WIFI_STATUS_DISABLED;
static int s_retry_count;
static char s_ip_address[16];
static bool s_started;


static bool credentials_are_configured(void)
{
    return APP_WIFI_HAS_LOCAL_CREDENTIALS &&
           APP_WIFI_SSID[0] != '\0' &&
           strcmp(APP_WIFI_SSID, "YOUR_WIFI_NAME") != 0;
}


static void wifi_event_handler(
    void *argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)argument;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_status = APP_WIFI_STATUS_CONNECTING;
        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", APP_WIFI_SSID);
        esp_err_t error = esp_wifi_connect();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(error));
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip_address[0] = '\0';
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_count < WIFI_MAXIMUM_RETRY) {
            ++s_retry_count;
            s_status = APP_WIFI_STATUS_CONNECTING;
            ESP_LOGW(
                TAG,
                "Wi-Fi disconnected, retry %d/%d",
                s_retry_count,
                WIFI_MAXIMUM_RETRY
            );
            esp_err_t error = esp_wifi_connect();
            if (error != ESP_OK) {
                ESP_LOGE(TAG, "Wi-Fi reconnect failed: %s", esp_err_to_name(error));
            }
        } else {
            s_status = APP_WIFI_STATUS_FAILED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
            ESP_LOGE(TAG, "Wi-Fi connection failed after retries");
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        snprintf(
            s_ip_address,
            sizeof(s_ip_address),
            IPSTR,
            IP2STR(&event->ip_info.ip)
        );
        s_retry_count = 0;
        s_status = APP_WIFI_STATUS_CONNECTED;
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAILED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected, IP address: %s", s_ip_address);
    }
}


static esp_err_t initialize_nvs(void)
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
        error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        error = nvs_flash_init();
    }
    return error;
}


esp_err_t app_wifi_start(void)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!credentials_are_configured()) {
        s_status = APP_WIFI_STATUS_DISABLED;
        ESP_LOGW(
            TAG,
            "Wi-Fi disabled: copy app_wifi_credentials.example.h to "
            "app_wifi_credentials.h and fill in local credentials"
        );
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(initialize_nvs(), TAG, "NVS initialization failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");

    esp_err_t error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Default event loop creation failed: %s", esp_err_to_name(error));
        return error;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_netif_t *station_netif = esp_netif_create_default_wifi_sta();
    if (station_netif == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&initialization), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &s_wifi_event_instance
        ),
        TAG,
        "Wi-Fi event registration failed"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &s_ip_event_instance
        ),
        TAG,
        "IP event registration failed"
    );

    wifi_config_t configuration = {0};
    strlcpy((char *)configuration.sta.ssid, APP_WIFI_SSID, sizeof(configuration.sta.ssid));
    strlcpy(
        (char *)configuration.sta.password,
        APP_WIFI_PASSWORD,
        sizeof(configuration.sta.password)
    );
    configuration.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    configuration.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_STA, &configuration),
        TAG,
        "set station config failed"
    );

    s_retry_count = 0;
    s_ip_address[0] = '\0';
    s_status = APP_WIFI_STATUS_CONNECTING;
    s_started = true;
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");
    return ESP_OK;
}


app_wifi_status_t app_wifi_get_status(void)
{
    return s_status;
}


bool app_wifi_is_connected(void)
{
    return s_status == APP_WIFI_STATUS_CONNECTED;
}


bool app_wifi_wait_connected(TickType_t timeout_ticks)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        timeout_ticks
    );
    return (bits & WIFI_CONNECTED_BIT) != 0;
}


bool app_wifi_get_ip(char *output, size_t output_size)
{
    if (output == NULL || output_size == 0 || !app_wifi_is_connected()) {
        return false;
    }
    strlcpy(output, s_ip_address, output_size);
    return true;
}
