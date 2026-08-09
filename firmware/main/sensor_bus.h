#ifndef FFD3D69F_7F23_420A_A582_C9279C4E3CC1
#define FFD3D69F_7F23_420A_A582_C9279C4E3CC1
#ifndef SENSOR_BUS_H
#define SENSOR_BUS_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/* 初始化外部传感器使用的第二路I2C */
esp_err_t sensor_bus_init(void);

/* 获取I2C总线句柄，后续传给两个传感器驱动 */
i2c_master_bus_handle_t sensor_bus_get_handle(void);

/* 扫描I2C总线上的设备 */
void sensor_bus_scan(void);

#endif

#endif /* FFD3D69F_7F23_420A_A582_C9279C4E3CC1 */
