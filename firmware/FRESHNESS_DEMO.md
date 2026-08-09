# 油桃新鲜度检测演示流程

当前固件使用 4 个位置、每个位置 20 帧的 AS7341 数据判断油桃是否存在新鲜度风险。
模型输出为 `fresh` 或 `risk`；DeepSeek 负责根据本地模型结果生成中文说明，不会覆盖本地判断。

## 实际操作

1. 开机后等待传感器、屏幕和 Wi-Fi 初始化完成。
2. 将油桃第一个位置放到传感器前方，保持 15–25 mm，按 KEY0 开始 P1。
3. 屏幕显示 `MEASURING` 时保持不动，直到显示 `P1 COMPLETE`。
4. 按提示将油桃完全移出检测距离。屏幕确认移开后，转动到 P2，重新放好并按 KEY0。
5. 依次完成 P2、P3、P4。每个位置都必须经过“移开—转动—放好—KEY0确认”；可疑软坑应作为其中一个位置。
6. P4 完成后，本地结果和置信度会停留在屏幕上，不会自动跳走。按 KEY0 才会请求 DeepSeek 报告。
7. DeepSeek 返回后显示 `SUCCESS` 和来源。按 KEY3 清除结果并开始测试下一颗水果。

## 按键

- KEY0：开始测试、确认采集下一面、结果完成后请求 AI 报告。
- KEY1：取消正在进行的采集，或重测刚完成的当前面。
- KEY2：在操作流程页和原始传感器数据页之间切换。
- KEY3：清除当前过程和结果，开始一次全新的测试。

KEY0～KEY3 连接在 XL9555 扩展 IO 的 P1_7～P1_4，并非直接连接 ESP32-S3 GPIO。
固件通过 I²C0（SDA GPIO41、SCL GPIO42）轮询读取按键，因此不需要配置按键中断跳线。

## 结果含义

- `fresh`：四位置总体未达到风险阈值。
- `risk`：四位置平均风险达到阈值，或任一位置出现高风险局部异常。
- `low / medium / high`：本地模型给出的风险等级。
- `local_anomaly`：某个位置风险概率达到 0.8，常用于提示软坑或局部腐坏。
- 置信度表示模型对 `fresh/risk` 结论的确定程度，不等同于“新鲜程度百分比”。

## 串口关键字段

`DATA` 行会额外输出：

- `freshness_state`
- `freshness_confidence`
- `freshness_risk`
- `risk_level`
- `freshness_position`
- `freshness_frames`
- `freshness_ready`
- `cloud_requested`
- `local_anomaly`

原有 F1–F8、Clear、NIR 和距离字段保持不变，Python 采集器仍可继续使用。
