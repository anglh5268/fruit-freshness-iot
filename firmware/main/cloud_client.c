#include "cloud_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"


#define CLOUD_PROVIDER_GATEWAY 1
#define CLOUD_PROVIDER_DEEPSEEK 2

#if APP_CLOUD_LOCAL_CONFIG
#include "cloud_gateway_config.h"
#endif

#ifndef APP_CLOUD_PROVIDER
#define APP_CLOUD_PROVIDER CLOUD_PROVIDER_GATEWAY
#endif

#ifndef APP_CLOUD_GATEWAY_URL
#define APP_CLOUD_GATEWAY_URL ""
#endif

#ifndef APP_CLOUD_DEVICE_ID
#define APP_CLOUD_DEVICE_ID "ESP32S3-01"
#endif

#ifndef APP_CLOUD_DEVICE_TOKEN
#define APP_CLOUD_DEVICE_TOKEN ""
#endif

#ifndef APP_CLOUD_DEEPSEEK_URL
#define APP_CLOUD_DEEPSEEK_URL "https://api.deepseek.com/v1/chat/completions"
#endif

#ifndef APP_CLOUD_DEEPSEEK_API_KEY
#define APP_CLOUD_DEEPSEEK_API_KEY ""
#endif

#ifndef APP_CLOUD_DEEPSEEK_MODEL
#define APP_CLOUD_DEEPSEEK_MODEL "deepseek-v4-flash"
#endif

#ifndef APP_CLOUD_HTTP_TIMEOUT_MS
#define APP_CLOUD_HTTP_TIMEOUT_MS 45000
#endif


typedef struct {
    char data[CLOUD_RESPONSE_BUFFER_SIZE];
    size_t length;
    bool overflow;
} response_buffer_t;


static const char *TAG = "CLOUD_CLIENT";


static bool using_deepseek(void)
{
    return APP_CLOUD_PROVIDER == CLOUD_PROVIDER_DEEPSEEK;
}


const char *cloud_client_provider_name(void)
{
    return using_deepseek() ? "deepseek" : "gateway";
}


static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    response_buffer_t *response = (response_buffer_t *)event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || response == NULL ||
        event->data == NULL || event->data_len <= 0) {
        return ESP_OK;
    }

    size_t available = sizeof(response->data) - 1U - response->length;
    if ((size_t)event->data_len > available) {
        response->overflow = true;
        return ESP_ERR_NO_MEM;
    }

    memcpy(response->data + response->length, event->data, event->data_len);
    response->length += (size_t)event->data_len;
    response->data[response->length] = '\0';
    return ESP_OK;
}


bool cloud_client_is_configured(void)
{
    if (!APP_CLOUD_LOCAL_CONFIG) {
        return false;
    }
    if (using_deepseek()) {
        return APP_CLOUD_DEEPSEEK_URL[0] != '\0' &&
               APP_CLOUD_DEEPSEEK_API_KEY[0] != '\0' &&
               APP_CLOUD_DEEPSEEK_MODEL[0] != '\0' &&
               strstr(APP_CLOUD_DEEPSEEK_API_KEY, "PASTE_") == NULL;
    }
    return APP_CLOUD_PROVIDER == CLOUD_PROVIDER_GATEWAY &&
           APP_CLOUD_GATEWAY_URL[0] != '\0' &&
           strstr(APP_CLOUD_GATEWAY_URL, "YOUR_PC_IP") == NULL;
}


esp_err_t cloud_client_analyze(
    const sensor_snapshot_t *snapshot,
    cloud_report_t *report
)
{
    if (snapshot == NULL || report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cloud_client_is_configured()) {
        return ESP_ERR_INVALID_STATE;
    }

    char request_body[CLOUD_REQUEST_BUFFER_SIZE];
    esp_err_t error;
    if (using_deepseek()) {
        error = cloud_protocol_build_deepseek_request(
            snapshot,
            APP_CLOUD_DEEPSEEK_MODEL,
            request_body,
            sizeof(request_body)
        );
    } else {
        error = cloud_protocol_build_request(
            snapshot,
            APP_CLOUD_DEVICE_ID,
            request_body,
            sizeof(request_body)
        );
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "JSON request build failed: %s", esp_err_to_name(error));
        return error;
    }

    const char *request_url = using_deepseek()
        ? APP_CLOUD_DEEPSEEK_URL
        : APP_CLOUD_GATEWAY_URL;
    response_buffer_t response = {0};
    esp_http_client_config_t configuration = {
        .url = request_url,
        .event_handler = http_event_handler,
        .user_data = &response,
        .timeout_ms = APP_CLOUD_HTTP_TIMEOUT_MS,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&configuration);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char authorization[384];
    if (using_deepseek()) {
        int authorization_length = snprintf(
            authorization,
            sizeof(authorization),
            "Bearer %s",
            APP_CLOUD_DEEPSEEK_API_KEY
        );
        if (authorization_length < 0 ||
            (size_t)authorization_length >= sizeof(authorization)) {
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_SIZE;
        }
        esp_http_client_set_header(client, "Authorization", authorization);
    } else if (APP_CLOUD_DEVICE_TOKEN[0] != '\0') {
        esp_http_client_set_header(client, "X-Device-Token", APP_CLOUD_DEVICE_TOKEN);
    }
    esp_http_client_set_post_field(client, request_body, (int)strlen(request_body));

    ESP_LOGI(
        TAG,
        "POST %s via %s (%u bytes)",
        request_url,
        using_deepseek() ? "deepseek" : "gateway",
        (unsigned)strlen(request_body)
    );
    error = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(error));
        return error;
    }
    if (response.overflow) {
        ESP_LOGE(TAG, "HTTP response exceeded %u bytes", (unsigned)sizeof(response.data));
        return ESP_ERR_INVALID_SIZE;
    }
    if (status_code != 200) {
        ESP_LOGE(TAG, "Cloud service returned HTTP status %d", status_code);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (using_deepseek()) {
        error = cloud_protocol_parse_deepseek_response(
            response.data,
            snapshot->sample_number,
            report
        );
    } else {
        error = cloud_protocol_parse_response(response.data, report);
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Cloud JSON response is invalid");
        return error;
    }
    ESP_LOGI(
        TAG,
        "%s response received: request_id=%s",
        using_deepseek() ? "DeepSeek" : "Gateway",
        report->request_id
    );
    return ESP_OK;
}
