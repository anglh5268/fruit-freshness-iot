# ESP32 直连 DeepSeek

最终演示链路：

```text
AS7341 + VL53L0X -> ESP32 本地模型 -> DeepSeek 官方 API -> TFT/串口报告
```

电脑只用于编译和烧录。设备运行时只需要连接能够访问互联网的 Wi-Fi 或手机热点。

## 1. 本地密钥配置

`main/cloud_gateway_config.h` 已被 `.gitignore` 忽略，真实 API Key 只能放在该文件中。

先复制 `main/cloud_gateway_config.example.h` 的内容，然后把配置改成：

```c
#define APP_CLOUD_PROVIDER 2
#define APP_CLOUD_DEEPSEEK_URL "https://api.deepseek.com/v1/chat/completions"
#define APP_CLOUD_DEEPSEEK_API_KEY "替换为比赛专用DeepSeek API Key"
#define APP_CLOUD_DEEPSEEK_MODEL "deepseek-v4-flash"
#define APP_CLOUD_HTTP_TIMEOUT_MS 45000
```

不要把 API Key 发到聊天、截图、串口日志或 Git 仓库。建议使用比赛专用 Key，账户只保留少量余额，比赛结束后删除该 Key。

## 2. 运行逻辑

1. ESP32 在 15~25 mm 内获得有效光谱和距离。
2. 本地模型置信度不低于 55%，并连续 10 帧识别为同一类别。
3. ESP32 向 DeepSeek 发送一次非流式 HTTPS 请求。
4. 使用 `deepseek-v4-flash`，关闭思考模式，并要求合法 JSON 输出。
5. ESP32 解析 `choices[0].message.content`，再解析其中的报告 JSON。
6. TFT 显示类别、置信度、风险等级和来源；中文摘要与建议写入串口日志。
7. 移走水果后才允许触发下一次检测，防止重复扣费。

当前边缘模型任务仍是 `fruit_identity`，因此云端报告必须返回 `risk_level=unknown`，不能声称已经判断新鲜度。完成新鲜度模型后再升级提示词和上报字段。

## 3. 编译与验证

在 VS Code 中依次执行：

1. `ESP-IDF: Build your project`
2. `ESP-IDF: Flash your project`
3. `ESP-IDF: Monitor your device`

成功日志类似：

```text
CLOUD_TASK: Stable detection: class=nectarine confidence=0.71 distance=19 mm
CLOUD_CLIENT: POST https://api.deepseek.com/v1/chat/completions via deepseek (... bytes)
CLOUD_CLIENT: DeepSeek response received: request_id=esp32-...
CLOUD_TASK: CLOUD_REPORT class=nectarine confidence=71 risk=unknown source=deepseek
```

Windows PowerShell 5.1 可能把 API 返回的 UTF-8 中文显示成乱码，这不代表 DeepSeek 返回错误。ESP32 的 cJSON 按 UTF-8 字节解析；当前 TFT 字库只显示 ASCII，中文报告主要通过串口或后续中文字库展示。

## 4. 切回原网关

如需使用已经验证过的电脑/veFaaS网关，把本地配置改为：

```c
#define APP_CLOUD_PROVIDER 1
#define APP_CLOUD_GATEWAY_URL "http://YOUR_PC_IP:8000/analyze"
#define APP_CLOUD_DEVICE_ID "ESP32S3-01"
#define APP_CLOUD_DEVICE_TOKEN "网关设备令牌"
```

两种模式共用同一个 `cloud_report_t` 和 TFT 显示流程。
