# ESP32-S3 多光谱与测距底层驱动

## 1. 项目功能

本项目基于 ESP32-S3 和 ESP-IDF v5.2.7，实现以下功能：

- 通过 I2C 读取 AS7341 多光谱传感器。
- 通过 I2C 读取 VL53L0X 距离传感器。
- 通过 SPI 驱动开发板 TFT LCD。
- 使用 FreeRTOS 分离传感器采集任务和屏幕显示任务。
- 在串口和 LCD 上显示距离、光谱通道及运行状态。

## 2. 硬件配置

### 外接传感器总线

- I2C控制器：I2C_NUM_1
- SDA：GPIO4
- SCL：GPIO5
- AS7341地址：0x39
- VL53L0X地址：0x29

### 开发板屏幕

LCD使用开发板BSP组件驱动，包括：

- SPI屏幕驱动
- XL9555扩展芯片
- 板载I2C_NUM_0
- GPIO41和GPIO42

## 3. 光谱通道

- F1：415 nm
- F2：445 nm
- F3：480 nm
- F4：515 nm
- F5：555 nm
- F6：590 nm
- F7：630 nm
- F8：680 nm
- Clear：全波段明通道
- NIR：近红外通道

## 4. FreeRTOS任务

### 传感器采集任务

文件：`main/sensor_task.c`

- 目标采集周期：100 ms
- 读取VL53L0X距离
- 读取AS7341光谱
- 使用互斥锁更新共享数据
- 通过`sensor_task_get_latest()`提供最新快照

### LCD显示任务

文件：`main/display_task.c`

- 检查周期：50 ms
- 获取最新传感器快照
- 只在出现新样本时刷新数值
- 显示距离、F1-F8、Clear、NIR和Sample

## 5. 主要接口

```c
esp_err_t sensor_bus_init(void);
void sensor_bus_scan(void);

esp_err_t as7341_init(void);
esp_err_t as7341_read_spectrum(as7341_spectral_data_t *data);

esp_err_t vl53l0x_init(void);
esp_err_t vl53l0x_read_distance(uint16_t *distance_mm);

esp_err_t sensor_task_start(void);
bool sensor_task_get_latest(sensor_snapshot_t *snapshot);

esp_err_t display_task_start(void);
```

## 6. 工程文件说明

- `main/main.c`：系统初始化和串口状态输出
- `main/sensor_bus.c/h`：外接传感器I2C总线
- `main/as7341.c/h`：AS7341驱动
- `main/vl53l0x.c/h`：VL53L0X应用层驱动
- `main/sensor_task.c/h`：传感器采集任务和共享快照
- `main/display_task.c/h`：LCD显示任务
- `components/vl53l0x_api`：ST官方VL53L0X API
- `components/BSP`：开发板LCD、SPI、I2C和XL9555驱动

## 7. 验收结果

- I2C扫描可以发现0x29和0x39。
- VL53L0X可以连续输出距离。
- AS7341可以连续输出F1-F8、Clear和NIR。
- LCD可以实时显示全部传感器数据。
- 传感器任务和显示任务能够长期稳定运行。
- 未出现看门狗复位、Guru Meditation或I2C超时报错。

## 8. 已知事项

VL53L0X在目标贴近传感器时存在近距离测量误差。当前不进行固定距离补偿，后续可以使用标准白色目标完成偏移校准。