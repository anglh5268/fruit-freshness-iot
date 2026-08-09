#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "sensor_bus.h"
#include "vl53l0x.h"
#include "as7341.h"
#include "sensor_task.h"
#include "display_task.h"
#include "app_wifi.h"
#include "cloud_task.h"
/* 正点原子板级驱动 */
#include "iic.h"
#include "spi.h"
#include "xl9555.h"
#include "lcd.h"


static const char *TAG = "APP";


void app_main(void)
{
    ESP_LOGI(TAG, "Project starting");

    /* 初始化板载XL9555和SPI屏幕 */
    ESP_LOGI(TAG, "Initializing LCD");

    i2c_obj_t board_i2c =
        iic_init(I2C_NUM_0);

    ESP_ERROR_CHECK(board_i2c.init_flag);

    spi2_init();
    xl9555_init(board_i2c);
    lcd_init();

    /* 第一次点屏测试 */
    lcd_clear(WHITE);

    lcd_show_string(
        30, 50,
        260, 32,
        32,
        "ESP32-S3",
        RED
    );

    lcd_show_string(
        30, 100,
        260, 32,
        32,
        "LCD OK",
        BLUE
    );

    ESP_LOGI(TAG, "LCD initialized");

    /* Wi-Fi is asynchronous and must not block local sensing. */
    esp_err_t wifi_error = app_wifi_start();
    if (wifi_error != ESP_OK && wifi_error != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Wi-Fi startup failed: %s", esp_err_to_name(wifi_error));
    }

    /* 初始化外接传感器I2C1 */
    ESP_ERROR_CHECK(sensor_bus_init());

    sensor_bus_scan();

    ESP_ERROR_CHECK(as7341_init());
    ESP_ERROR_CHECK(vl53l0x_init());
    ESP_ERROR_CHECK(sensor_task_start());
    ESP_ERROR_CHECK(display_task_start());
    ESP_ERROR_CHECK(cloud_task_start());
    ESP_LOGI(TAG, "Initialization completed");

    while (1) {
        sensor_snapshot_t snapshot;

        bool received =
            sensor_task_get_latest(&snapshot);

        if (!received ||
            snapshot.sample_number == 0) {

            ESP_LOGI(
                TAG,
                "Waiting for first sensor sample"
            );

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (snapshot.spectrum_valid) {
            const int distance_mm =
                snapshot.distance_valid
                    ? (int)snapshot.distance_mm
                    : -1;
            const char *class_label =
                snapshot.classification.valid
                    ? snapshot.classification.label
                    : "invalid";
            const unsigned int confidence_percent =
                snapshot.classification.valid
                    ? (unsigned int)(
                        snapshot.classification.confidence * 100.0f + 0.5f
                    )
                    : 0U;
            const char *freshness_label =
                freshness_classifier_label(&snapshot.freshness);
            const unsigned int freshness_confidence_percent =
                snapshot.freshness.result_valid
                    ? (unsigned int)(
                        snapshot.freshness.confidence * 100.0f + 0.5f
                    )
                    : 0U;
            const unsigned int freshness_risk_percent =
                snapshot.freshness.result_valid
                    ? (unsigned int)(
                        snapshot.freshness.risk_probability * 100.0f + 0.5f
                    )
                    : 0U;

            /*
             * F1~NIR是净反射值，保持旧版Python采集器兼容。
             * Ambient_*和Lit_*保留关灯/开灯原始值，便于校准。
             */
            printf(
                "DATA,sample=%lu,distance_mm=%d,class=%s,confidence=%u,"
                "freshness_state=%s,freshness_confidence=%u,"
                "freshness_risk=%u,risk_level=%s,"
                "freshness_position=%u,freshness_frames=%u,"
                "freshness_ready=%u,cloud_requested=%u,local_anomaly=%u,"
                "F1_415nm=%u,F2_445nm=%u,F3_480nm=%u,F4_515nm=%u,"
                "F5_555nm=%u,F6_590nm=%u,F7_630nm=%u,F8_680nm=%u,"
                "Clear=%u,NIR=%u,"
                "Ambient_F1_415nm=%u,Ambient_F2_445nm=%u,"
                "Ambient_F3_480nm=%u,Ambient_F4_515nm=%u,"
                "Ambient_F5_555nm=%u,Ambient_F6_590nm=%u,"
                "Ambient_F7_630nm=%u,Ambient_F8_680nm=%u,"
                "Ambient_Clear=%u,Ambient_NIR=%u,"
                "Lit_F1_415nm=%u,Lit_F2_445nm=%u,"
                "Lit_F3_480nm=%u,Lit_F4_515nm=%u,"
                "Lit_F5_555nm=%u,Lit_F6_590nm=%u,"
                "Lit_F7_630nm=%u,Lit_F8_680nm=%u,"
                "Lit_Clear=%u,Lit_NIR=%u\r\n",
                (unsigned long)snapshot.sample_number,
                distance_mm,
                class_label,
                confidence_percent,
                freshness_label,
                freshness_confidence_percent,
                freshness_risk_percent,
                freshness_classifier_risk_level_text(
                    snapshot.freshness.risk_level
                ),
                (unsigned int)snapshot.freshness.current_position,
                (unsigned int)snapshot.freshness.frames_collected,
                snapshot.freshness.result_valid ? 1U : 0U,
                snapshot.freshness.cloud_requested ? 1U : 0U,
                snapshot.freshness.local_anomaly_detected ? 1U : 0U,
                (unsigned int)snapshot.spectrum.f1_415nm,
                (unsigned int)snapshot.spectrum.f2_445nm,
                (unsigned int)snapshot.spectrum.f3_480nm,
                (unsigned int)snapshot.spectrum.f4_515nm,
                (unsigned int)snapshot.spectrum.f5_555nm,
                (unsigned int)snapshot.spectrum.f6_590nm,
                (unsigned int)snapshot.spectrum.f7_630nm,
                (unsigned int)snapshot.spectrum.f8_680nm,
                (unsigned int)snapshot.spectrum.clear,
                (unsigned int)snapshot.spectrum.nir,
                (unsigned int)snapshot.ambient_spectrum.f1_415nm,
                (unsigned int)snapshot.ambient_spectrum.f2_445nm,
                (unsigned int)snapshot.ambient_spectrum.f3_480nm,
                (unsigned int)snapshot.ambient_spectrum.f4_515nm,
                (unsigned int)snapshot.ambient_spectrum.f5_555nm,
                (unsigned int)snapshot.ambient_spectrum.f6_590nm,
                (unsigned int)snapshot.ambient_spectrum.f7_630nm,
                (unsigned int)snapshot.ambient_spectrum.f8_680nm,
                (unsigned int)snapshot.ambient_spectrum.clear,
                (unsigned int)snapshot.ambient_spectrum.nir,
                (unsigned int)snapshot.illuminated_spectrum.f1_415nm,
                (unsigned int)snapshot.illuminated_spectrum.f2_445nm,
                (unsigned int)snapshot.illuminated_spectrum.f3_480nm,
                (unsigned int)snapshot.illuminated_spectrum.f4_515nm,
                (unsigned int)snapshot.illuminated_spectrum.f5_555nm,
                (unsigned int)snapshot.illuminated_spectrum.f6_590nm,
                (unsigned int)snapshot.illuminated_spectrum.f7_630nm,
                (unsigned int)snapshot.illuminated_spectrum.f8_680nm,
                (unsigned int)snapshot.illuminated_spectrum.clear,
                (unsigned int)snapshot.illuminated_spectrum.nir
            );
            fflush(stdout);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
