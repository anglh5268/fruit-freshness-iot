# 四号位工作交接说明

## 项目目标

设备使用 AS7341、VL53L0X 和 ESP32-S3 完成稳定测量，本地模型先给出快速判断，
云端大模型再把本地判断和光谱数据解释成简短、易懂的风险说明，最后显示在 TFT 屏幕上。
大模型负责“解释”，不代替经过验证的本地模型直接猜测水果是否腐败。

## 与三号位并行工作的规则

三号位继续维护：

- `src/serial_collect.py` 和串口采集流程；
- `src/analyze_freshness_dataset.py` 和新鲜度纵向分析；
- 训练、验证、导出 ESP32 模型；
- 光谱特征、标签、置信度的定义。

四号位维护：

- `cloud_gateway/` 电脑端 HTTP 网关；
- ESP 工程中后续新增的 `app_wifi`、`cloud_protocol`、`cloud_client`、`cloud_task`；
- TFT 中的联网状态、云端分析状态和报告显示；
- 豆包 API 的调用与密钥管理。

若接口字段需要变化，应先更新 `cloud_gateway/schemas.py`、示例 JSON 和测试，再同步 ESP32。
不得直接修改三号位的数据含义，也不得把 API 密钥提交到 Git。

## 端云数据流

```text
AS7341/VL53L0X
      |
sensor_task -> 本地模型与稳定投票 -> cloud_task -> HTTP/JSON
                                            |
                                     电脑 Python 网关
                                            |
                                      豆包大模型（后接）
                                            |
display_task <- 云端结构化报告 <-------------+
```

`sensor_task` 不能直接执行 HTTP。稳定结果通过队列交给独立的 `cloud_task`，防止网络超时
阻塞传感器采集。建议一次检测连续稳定 10~20 帧后只上传一次，并使用
`IDLE/PENDING/REQUESTING/SUCCESS/TIMEOUT/ERROR` 状态机。

## 当前完成情况

- 已确定协议版本 `protocol_version=1`；
- 已实现 `GET /health` 和 `POST /analyze`；
- 已支持当前 `fruit_identity` 与未来 `freshness` 两种任务；
- 已实现严格字段检查、错误 JSON 和离线测试；
- 已实现 `mock/ark` 双模式；Ark 模式使用官方 Responses API、Bearer API Key 和严格报告校验；
- 本地 `.env` 仍需由用户填写实际 API Key 与模型ID，完成首次真实调用验证；
- 尚未修改 ESP32 工程，三号位固件不受本次提交影响。

## 后续顺序

1. 启动并用电脑验证 mock 网关；
2. 给 ESP32 增加 Wi-Fi Station 和联网状态；
3. 给 ESP32 增加 HTTP/cJSON 客户端，对接 mock 网关；
4. 将云端报告显示到现有 TFT 驱动，先不迁移 LVGL；
5. 查阅火山引擎官方最新接口文档，在网关接入豆包；
6. 做断网、超时、错误 JSON、低置信度等降级测试；
7. 基本闭环稳定后，再决定是否用 LVGL 美化界面。

查看每次四号位提交可运行：

```powershell
git log --oneline -- cloud_gateway ROLE4_HANDOFF.md
```
