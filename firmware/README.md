# ESP32-S3 固件

该目录是完整 ESP-IDF 工程，负责传感器采集、距离门控、四位置测量、本地模型推理、
TFT 中文交互以及 DeepSeek HTTPS 请求。

## 环境

- ESP-IDF 5.2.x
- 目标芯片：ESP32-S3
- 16 MiB Flash 分区配置

## 本地配置

```powershell
Copy-Item main\app_wifi_credentials.example.h main\app_wifi_credentials.h
Copy-Item main\cloud_gateway_config.example.h main\cloud_gateway_config.h
```

真实配置文件已被 Git 忽略。DeepSeek 模式下在 `cloud_gateway_config.h` 中选择 provider 2，
填写官方 API 地址、模型名和比赛专用 API Key。

## 编译烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## 关键模块

- `main/as7341.*`：多光谱采集与补光控制
- `main/vl53l0x.*`：距离测量
- `main/sensor_task.*`：采样、质量控制和结构化串口输出
- `main/freshness_classifier.*`：四位置本地新鲜度风险推理
- `main/nect_freshness_model.h`：Python 导出的模型参数
- `main/display_task.*`：TFT 中文流程与结果页
- `main/cloud_client.*`：HTTPS 请求
- `main/cloud_protocol.*`：DeepSeek JSON 构造与解析
- `main/cloud_task.*`：稳定触发、去重、重试与结果状态

现场操作见 [`FRESHNESS_DEMO.md`](FRESHNESS_DEMO.md)。

