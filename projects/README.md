# AirMix 固件子项目入口

双板原型：ESP32（蓝牙 PCM 源）+ ESP32-P4（I2S 接收 + ES8311 播放）。

| 路径 | 芯片 | 用途 |
|------|------|------|
| `projects/esp32-a2dp` | **esp32** | A2DP → I2S Master TX → P4 |
| `firmware/esp32p4-bt-bridge-rx` | **esp32p4** | I2S Slave RX → ES8311 喇叭 |

刷机步骤见 [`docs/esp32_p4_bt_playback.md`](../docs/esp32_p4_bt_playback.md)。
