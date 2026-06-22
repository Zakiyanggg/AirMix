# AirMix 子项目入口

左下角 workspace 通过**不同路径**区分固件项目（避免 Cursor 合并同名目录）：

| 路径 | 用途 | 刷机 |
|------|------|------|
| `projects/esp32-bt-audio` | ESP32 蓝牙 A2DP + 小屏 | **esp32** |
| `projects/esp32c6-display` | ESP32-C6 1.47 寸大屏 | **esp32c6** |

两者软链接到同一份源码，构建目录分别为 `build-esp32` / `build-esp32c6`。
