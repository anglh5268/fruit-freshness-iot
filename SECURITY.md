# 安全说明

## 禁止提交的内容

- DeepSeek 或其他模型服务的 API Key
- Wi-Fi SSID 与密码
- 个人访问令牌、GitHub Token
- 包含密钥的 `.env` 文件

本地固件配置文件：

- `firmware/main/app_wifi_credentials.h`
- `firmware/main/cloud_gateway_config.h`

已经由仓库根目录和固件目录的 `.gitignore` 排除。只提交对应的 `.example.h` 示例。

## 比赛演示建议

1. 为比赛创建独立的 DeepSeek API Key。
2. 账户只保留少量演示余额，并限制调用频率。
3. 不在串口日志、截图、视频或提交记录中展示密钥。
4. 比赛结束后立即删除或轮换演示 Key。
5. 正式部署应使用服务端网关保存密钥，而不是把付费密钥固化在设备中。

