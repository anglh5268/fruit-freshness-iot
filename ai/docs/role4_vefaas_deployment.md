# 四号位：将网关部署到 veFaaS

目标链路：

```text
ESP32-S3 -> 手机热点或普通 Wi-Fi -> 公网 HTTPS 网关 -> 火山方舟 -> ESP32 屏幕
```

完成后，比赛演示时不需要电脑运行 Python 网关。方舟 API Key 仅保存在云端，ESP32 只保存权限较低的设备令牌。

## 1. 生成部署压缩包

在 VS Code 中打开 `spectrum_ai`，新建 PowerShell 终端并运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_vefaas_package.ps1
```

这里的 `Bypass` 只对本次命令生效，不会修改系统的全局执行策略。

输出文件为：

```text
dist/spectrum_gateway_vefaas.zip
```

脚本使用 Windows 自带的 `tar.exe` 生成 ZIP，只打包 `cloud_gateway` 中的 Python 源码，不会打包 `.env` 或 API Key。脚本还会自动检查 ZIP 内部路径，正确输出必须类似：

```text
cloud_gateway/server.py
cloud_gateway/settings.py
```

不能出现 `cloud_gateway\server.py`。Windows PowerShell 的 `Compress-Archive` 可能把反斜杠写进 ZIP，veFaaS/Linux 会将其识别成普通文件名，进而导致 `No module named 'cloud_gateway'`，因此本项目不再使用 `Compress-Archive` 打包。

## 2. 创建 veFaaS Python Web 应用

在火山引擎函数服务 veFaaS 控制台创建 Python Web 应用，上传上一步生成的 zip。启动命令填写：

```text
python3 -m cloud_gateway.server
```

程序自动读取平台提供的 `_FAAS_RUNTIME_PORT`，不需要把端口写死为 8000。

上传后先在 veFaaS“代码”页面确认左侧出现可以展开的 `cloud_gateway` 文件夹，再发布 `Latest`。如果代码列表显示 `cloud_gateway\server.py` 这样的单个文件名，说明仍然上传了旧包。

## 3. 配置云端环境变量

在 veFaaS 环境变量中填写：

```text
CLOUD_GATEWAY_MODE=ark
ARK_API_KEY=你的实际方舟APIKey
ARK_MODEL_ID=deepseek-v4-flash-260425
ARK_BASE_URL=https://ark.cn-beijing.volces.com/api/v3
ARK_TIMEOUT_SECONDS=30
DEVICE_API_TOKEN=一段至少32位的随机字符串
```

不要把 `ARK_API_KEY` 发到聊天、写入 Git 或烧进 ESP32。

## 4. 验证公网接口

发布函数并取得 HTTPS 域名后，先在电脑执行：

```powershell
Invoke-RestMethod https://你的公网域名/health
```

应看到 `ok=True`、`mode=ark`、`configured=True` 和 `device_auth_required=True`。

## 5. 配置 ESP32

修改被 Git 忽略的 `ESP-project/main/cloud_gateway_config.h`：

```c
#define APP_CLOUD_GATEWAY_URL "https://你的公网域名/analyze"
#define APP_CLOUD_DEVICE_ID "ESP32S3-01"
#define APP_CLOUD_DEVICE_TOKEN "与云端DEVICE_API_TOKEN完全相同"
```

重新构建、烧录并监视串口。ESP32 已使用 ESP-IDF 系统 CA 证书包验证 HTTPS 服务器证书。

## 6. 脱离电脑验证

关闭电脑上的 Python 网关，ESP32 和手机热点保持连接。重新放置水果，串口应出现公网 HTTPS POST、方舟响应和 `CLOUD_REPORT`，屏幕应进入 `AI CLOUD REPORT` 页面。
