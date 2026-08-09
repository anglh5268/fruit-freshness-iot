#ifndef D20847E7_2D34_4984_9D8D_290DD3813509
#define D20847E7_2D34_4984_9D8D_290DD3813509
#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "as7341.h"
#include "freshness_classifier.h"
#include "material_classifier.h"


typedef struct {
    uint32_t sample_number;

    bool distance_valid;
    uint16_t distance_mm;

    bool spectrum_valid;
    /* 净反射光，供屏幕显示、串口采集和模型使用。 */
    as7341_spectral_data_t spectrum;
    /* 保留原始关灯与开灯数据，便于校准和排查。 */
    as7341_spectral_data_t ambient_spectrum;
    as7341_spectral_data_t illuminated_spectrum;
    material_classification_t classification;
    freshness_classification_t freshness;
    bool ui_show_sensor_details;
} sensor_snapshot_t;


/* 创建传感器采集任务 */
esp_err_t sensor_task_start(void);

/* 获取最新的一份传感器数据 */
bool sensor_task_get_latest(
    sensor_snapshot_t *snapshot
);

#endif

#endif /* D20847E7_2D34_4984_9D8D_290DD3813509 */
