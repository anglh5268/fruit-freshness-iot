# 四号位本地云网关

此目录属于四号位，负责 ESP32 与云端大模型之间的 HTTP/JSON 接口。当前是 `mock` 模式：
返回固定规则生成的报告，用于先打通 ESP32 联网、上传、解析和屏幕显示。三号位的数据采集、
新鲜度分析和模型训练仍在 `src/` 中，二者可以并行工作。

## 启动

在 VS Code 中打开 `spectrum_ai` 文件夹，新建一个终端：

```powershell
.\.venv\Scripts\python.exe -m cloud_gateway.server --host 0.0.0.0 --port 8000
```

电脑防火墙若询问是否允许 Python 访问专用网络，选择允许。ESP32 和电脑必须连接同一个 Wi-Fi。

## 电脑端自测

另开一个 PowerShell 终端：

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health
```

发送示例测量：

```powershell
$body = Get-Content .\cloud_gateway\example_request.json -Raw
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/analyze -ContentType "application/json" -Body $body
```

成功时会返回 `ok=true`、`risk_level`、`summary`、`advice` 和 `model_source=mock`。

## 三号位与四号位的接口边界

- 三号位提供：距离、10 通道净反射数据、本地任务、本地标签和 0~1 置信度。
- 四号位负责：Wi-Fi、HTTP、JSON、云网关、大模型解释和屏幕中的云端状态。
- `task=fruit_identity` 可立即对接当前水果分类模型。
- 将来新鲜度模型完成后只改为 `task=freshness`，标签使用 `fresh/warning/spoiled`。
- ESP32 不应每 100 ms 上传；应等待本地结果连续稳定后，一次检测只上传一次。
- API 密钥只放电脑网关的 `.env`，绝不写进 ESP32、代码仓库或聊天消息。

协议详情以 `schemas.py` 和 `example_request.json` 为准。未来接入豆包时必须保持响应字段兼容，
这样 ESP32 端不需要重写。

## 切换到真实火山方舟

本机已经创建了被 Git 忽略的 `cloud_gateway/.env`。打开它并填写：

```text
CLOUD_GATEWAY_MODE=ark
ARK_API_KEY=这里填写控制台中可复制的实际API Key
ARK_MODEL_ID=这里填写doubao-...模型ID或ep-...推理接入点ID
ARK_BASE_URL=https://ark.cn-beijing.volces.com/api/v3
ARK_TIMEOUT_SECONDS=30
```

`apikey-日期-后缀`通常是 API Key 资源名称，不是 `ARK_MODEL_ID`，也不一定是实际密钥值。
在火山方舟体验中心选择模型后打开“开发者模式”，从示例请求的 `model` 字段复制模型ID。
实际 API Key 不得发到聊天、写入 ESP32 或提交 Git。

修改 `.env` 后先按 `Ctrl+C` 停止旧网关，再重新启动：

```powershell
.\.venv\Scripts\python.exe -m cloud_gateway.server --host 0.0.0.0 --port 8000
```

检查状态：

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health
```

真实模式应显示 `mode=ark`、`configured=True` 和所选模型ID。随后可先在电脑发送示例：

```powershell
$body = Get-Content .\cloud_gateway\example_request.json -Raw
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/analyze -ContentType "application/json" -Body $body
```

返回 `model_source=ark` 才代表真实模型调用成功。当前边缘任务仍是水果种类识别，因此报告的
`risk_level` 应为 `unknown`；在新鲜度模型完成之前，大模型不得声称水果新鲜或腐败。

如需退回不计费的离线模式，将 `.env` 中的 `CLOUD_GATEWAY_MODE` 改回 `mock` 并重启网关。
