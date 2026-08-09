# 四号位：ESP32-S3 Wi-Fi 配置与验证

## 当前阶段

固件已加入独立的 Wi-Fi Station 模块。它只负责连接路由器并获取 IP，暂时还没有发送 HTTP。
Wi-Fi 断开或尚未配置时，AS7341、VL53L0X、本地分类和串口采集仍然继续运行。

## 第一次配置

1. 在 VS Code 左侧打开 `ESP-project/main`。
2. 找到 `app_wifi_credentials.example.h`。
3. 在同一目录复制一份，将副本改名为 `app_wifi_credentials.h`。
4. 打开副本，只修改下面两项：

```c
#define APP_WIFI_SSID "你的Wi-Fi名称"
#define APP_WIFI_PASSWORD "你的Wi-Fi密码"
```

真实的 `app_wifi_credentials.h` 已被 `.gitignore` 忽略，不会加入 Git。不要修改示例文件，
也不要把密码发到聊天中。

编辑完成后必须按 `Ctrl+S` 保存，再重新执行 `Build Project` 和 `Flash Device`。只重新打开串口
监视器不会改变已经烧录的旧固件。

ESP32 只能连接 2.4 GHz Wi-Fi。若使用手机热点，请开启兼容模式或选择 2.4 GHz 频段。

## 编译、烧录和观察

使用 ESP-IDF 扩展底部状态栏依次执行：

1. `Build Project`
2. `Flash Device`
3. `Monitor Device`

连接成功时串口应出现：

```text
I (...) APP_WIFI: Connecting to Wi-Fi SSID: ...
I (...) APP_WIFI: Wi-Fi connected, IP address: 192.168.x.x
```

把这个 IP 记下来即可。但下一阶段真正需要的是电脑的局域网 IP，因为 ESP32 将访问电脑上运行的
Python 网关。

如果看到 `Wi-Fi connection failed after retries`：

- 检查账号密码是否正确；
- 检查热点是否为 2.4 GHz；
- 检查信号强度；
- 修改凭据后重新编译并烧录。

如果看到 `Wi-Fi disabled`，说明文件名不是准确的 `app_wifi_credentials.h`，或者仍然保留了
`YOUR_WIFI_NAME` 占位符。
