#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include "esp_err.h"

/* 初始化VL53L0X */
esp_err_t vl53l0x_init(void);
/* 执行距离偏移校准，结果单位为微米 */
esp_err_t vl53l0x_calibrate_offset(
    uint16_t known_distance_mm,
    int32_t *offset_um
);
/* 读取距离，单位为毫米 */
esp_err_t vl53l0x_read_distance(uint16_t *distance_mm);

#endif