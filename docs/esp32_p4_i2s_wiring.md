# ESP32 → ESP32-P4 I2S 接线

> 阶段 A：ESP32 收 A2DP，I2S Master 输出 PCM；P4 侧下一步再做 I2S Slave 接收 + ES8311 播放。

## 信号说明

| 信号 | 方向 | 格式 |
|------|------|------|
| BCLK | ESP32 → P4 | I2S 位时钟 |
| WS (LRCK) | ESP32 → P4 | 左右声道帧时钟 |
| DOUT | ESP32 → P4 | PCM 16-bit，Philips I2S |
| GND | 共地 | **必须连接** |

ESP32 为 **I2S Master**，P4 下一步配 **I2S Slave** 跟随时钟。

采样率随手机 A2DP 协商，常见 **44100 Hz 立体声**（也可能是 48000 或 mono，ESP32 会自动重配 I2S）。

## 接线表

| ESP32 GPIO | 信号 | 杜邦线 | ESP32-P4 GPIO | 说明 |
|------------|------|--------|---------------|------|
| **GPIO 27** | BCLK | ────── | **GPIO 2** | 位时钟 |
| **GPIO 14** | WS / LRCK | ────── | **GPIO 3** | 字选择 |
| **GPIO 22** | DOUT (SD) | ────── | **GPIO 4** | 音频数据 → P4 输入 |
| **GND** | 地 | ────── | **GND** | 共地，必接 |

### 不要使用的 P4 引脚

| GPIO | 原因 |
|------|------|
| 7, 8 | ES8311 I2C |
| 9–13 | ES8311 I2S（板载 codec） |
| 14–19 | 板载 C6 SDIO（Wi-Fi） |
| 53 | ES8311 功放使能 |
| 54 | C6 复位 |

P4 侧 GPIO 2/3/4 为 **I2S1 外部输入预留**，与 ES8311（I2S0）和 SDIO 不冲突。

### ESP32 上可保留的其他外设

| 功能 | GPIO | 与 I2S 冲突 |
|------|------|-------------|
| 1.8 寸 TFT | 18,23,5,16,17,4 | ❌ 不冲突 |
| 按键 K1–K4 | 32,33,25,26 | ❌ 不冲突 |
| ~~MAX98357A SD~~ | ~~21~~ | P4 模式下不用 |

**MAX98357A 可以不接**；GPIO 27/14/22 改接到 P4。

## 元数据 UART（P4 屏幕显示歌名）

除 I2S 音频外，再加 **1 根信号线**：

| ESP32 GPIO | 信号 | P4 丝印 |
|------------|------|---------|
| **GPIO 17** | TX | **GPIO27** (RX) |
| **GND** | | **GND** |

115200 8N1。两板都需刷含 `bt_meta_uart` / `meta_uart_rx` 的固件。详见 [esp32_p4_spi_lcd_wiring.md](esp32_p4_spi_lcd_wiring.md)。

## 刷 ESP32 固件

在 workspace **① ESP32** 下：

```bash
idf.py set-target esp32
idf.py build flash monitor
```

串口应看到类似：

```text
P4 bridge I2S Master TX: BCLK=GPIO27 WS=GPIO14 DOUT=GPIO22 -> P4 BCLK=GPIO2 WS=GPIO3 DIN=GPIO4
Ready. Pair phone with "ESP32-AUDIO" and play music.
```

连接手机蓝牙 **ESP32-AUDIO**，播放音乐后 `i2s_write_count` 会递增（小屏 UI 或日志可见）。

## 验证（P4 固件完成前）

P4 还没刷接收固件时，ESP32 仍正常连蓝牙、向 I2S 线发 PCM；P4 喇叭暂时无声是正常的。

可用逻辑分析仪 / 示波器看 GPIO 27 是否有 BCLK 波形确认。

## 恢复 MAX98357A 本地播放

`main/BT_Player/BT_Player.h` 中改：

```c
#define BT_PLAYER_OUTPUT_P4  0
```

重新编译即可回到旧方案。
