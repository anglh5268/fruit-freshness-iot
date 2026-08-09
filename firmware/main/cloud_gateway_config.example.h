#ifndef CLOUD_GATEWAY_CONFIG_H
#define CLOUD_GATEWAY_CONFIG_H

/*
 * Copy this file to cloud_gateway_config.h. The copied file is ignored by Git.
 * Provider 1 = the existing PC/veFaaS gateway.
 * Provider 2 = direct DeepSeek official API (recommended for standalone demo).
 */
#define APP_CLOUD_PROVIDER 2

/* Direct DeepSeek configuration. Never commit a real API key. */
#define APP_CLOUD_DEEPSEEK_URL "https://api.deepseek.com/v1/chat/completions"
#define APP_CLOUD_DEEPSEEK_API_KEY "PASTE_DEEPSEEK_API_KEY_HERE"
#define APP_CLOUD_DEEPSEEK_MODEL "deepseek-v4-flash"

/* Existing gateway fallback configuration. */
#define APP_CLOUD_GATEWAY_URL "http://YOUR_PC_IP:8000/analyze"
#define APP_CLOUD_DEVICE_ID "ESP32S3-01"
#define APP_CLOUD_DEVICE_TOKEN "PASTE_THE_SAME_DEVICE_API_TOKEN_HERE"

#define APP_CLOUD_HTTP_TIMEOUT_MS 45000

#endif
