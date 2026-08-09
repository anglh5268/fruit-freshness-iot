# 多光谱数据与边缘模型工作区

该目录负责 AS7341 串口数据采集、质量审计、跨天变化分析、新鲜度模型训练、实时验证和
ESP32 C 头文件导出。

## 安装

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe src\check_environment.py
```

## 测试

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

## 主要程序

- `src/serial_collect.py`：串口采集与元数据记录
- `src/analyze_repeatability.py`：重复性和通道 CV 分析
- `src/analyze_freshness_dataset.py`：跨天数据选择、审计和趋势汇总
- `src/train_freshness_classifier.py`：留一水果验证、模型训练和 ESP32 参数导出
- `src/predict_freshness_live.py`：四位置实时模型验证
- `cloud_gateway/`：旧电脑/云函数网关备用实现

## 发布数据

`data/processed/` 包含位置级汇总、每日汇总、完整性检查和距离补偿对照。原始逐帧 CSV
不随公开代码仓库发布。`figures/` 包含模型评估和跨天趋势图。

## 模型训练

```powershell
.\.venv\Scripts\python.exe src\train_freshness_classifier.py `
  --esp-header "..\firmware\main\nect_freshness_model.h"
```

模型用途、指标和限制见 [`models/NECT_FRESHNESS_V1_model_card.md`](models/NECT_FRESHNESS_V1_model_card.md)。
