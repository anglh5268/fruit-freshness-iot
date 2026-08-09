# 四号位：ESP32 到电脑模拟云网关联调

## 数据流

```text
AS7341 + VL53L0X -> 本地水果模型 -> cloud_task -> HTTP/JSON
                                                   |
                                             电脑Python网关
                                                   |
                                              模拟中文报告
```

`sensor_task` 不执行网络操作。`cloud_task` 每 200 ms 查看一次新快照，仅当以下条件同时满足时上传：

- ESP32 已连接 Wi-Fi；
- 距离处于 15~25 mm；
- 本地模型结果有效，置信度不低于 55%；
- 同一类别连续稳定 10 次；
- 当前这次检测尚未上传。

把水果移出有效距离后，再放入可触发下一次检测。网络失败时等待 5 秒再重试。

## 1. 让电脑和 ESP32 进入同一网络

如果 ESP32 连接 `iPhone`，电脑也必须连接同一个 `iPhone` 热点。只访问 `127.0.0.1` 不能证明
ESP32 可以访问电脑。

电脑切换 Wi-Fi 后，在 PowerShell 查询电脑 IPv4：

```powershell
Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notlike "169.254*" }
```

记下类似 `172.20.10.2` 的 `IPAddress`。不要使用 `127.0.0.1`，那只代表设备自身。

## 2. 配置 ESP32 网关地址

打开 `main/cloud_gateway_config.h`，将：

```c
#define APP_CLOUD_GATEWAY_URL "http://YOUR_PC_IP:8000/analyze"
```

改为电脑的实际 IPv4，例如：

```c
#define APP_CLOUD_GATEWAY_URL "http://172.20.10.2:8000/analyze"
```

该本地文件已被 Git 忽略。每次电脑网络变化后，IPv4 可能变化，需要重新确认。

## 3. 启动电脑网关

在 `spectrum_ai` 文件夹打开第一个终端并保持运行：

```powershell
.\.venv\Scripts\python.exe -m cloud_gateway.server --host 0.0.0.0 --port 8000
```

另开终端测试 localhost：

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health
```

再用电脑真实 IPv4 测试，例如：

```powershell
Invoke-RestMethod http://172.20.10.2:8000/health
```

两次都应返回 `ok=True`。Windows 防火墙询问时允许 Python 访问专用网络。

## 4. 编译、烧录、监视

保存 `cloud_gateway_config.h` 后依次执行：

1. `ESP-IDF: Build your project`
2. `ESP-IDF: Flash your project`
3. `ESP-IDF: Monitor your device`

把香蕉、毛桃或油桃稳定放在 15~25 mm，保持约 2 秒。成功日志类似：

```text
CLOUD_TASK: Stable detection: class=banana confidence=0.72 distance=20 mm
CLOUD_CLIENT: POST http://172.20.10.2:8000/analyze (... bytes)
CLOUD_CLIENT: Gateway response received: request_id=esp32-123
CLOUD_TASK: CLOUD_REPORT class=banana confidence=72 risk=low source=mock
CLOUD_TASK: CLOUD_SUMMARY 本地模型识别为香蕉
CLOUD_TASK: CLOUD_ADVICE 当前仅验证水果类别，腐败判断需等待新鲜度模型
```

电脑网关终端同时应出现：

```text
[gateway] ... "POST /analyze HTTP/1.1" 200 -
```

TFT 平时保持光谱页面，并在底部显示 `Cloud: READY`。达到稳定条件后会自动切换为
`AI CLOUD REPORT` 页面，显示 `STATUS / FRUIT / CONF / RISK / SOURCE`。当前LCD字库只支持
ASCII，所以中文报告保留在串口中；拿走水果后屏幕自动回到光谱页面，可以开始下一次检测。

## 常见错误

- `Cloud upload disabled`：`cloud_gateway_config.h` 仍含 `YOUR_PC_IP`，或文件名不正确。
- `HTTP request failed: ESP_ERR_HTTP_CONNECT`：电脑和 ESP32 不在同一网络、IP 填错或防火墙阻止。
- 一直没有 `Stable detection`：距离不在 15~25 mm、类别无效或置信度低于 55%。
- HTTP 状态不是 200：查看 Python 网关终端中的请求错误。
- 收到报告后不再上传：这是防重复设计；先把物体移出有效距离，再重新放入。
