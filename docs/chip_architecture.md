# AirMix 双板原型架构

> 硬件：**ESP32 Classic**（蓝牙 PCM 桥）+ **ESP32-P4-WIFI6**（音频引擎 + UI + Melody）

## 目标架构（推荐）

```text
         手机 (A2DP)
              │
   ┌──────────▼──────────┐
   │   ESP32 Classic     │  ① 仅蓝牙：A2DP 解码 + AVRC 元数据
   │   I2S Master TX ────┼── PCM 流 ──►
   └──────────┬──────────┘              │
              │ UART（元数据/鼓垫触发）   │
   ┌──────────▼──────────▼──────────┐
   │         ESP32-P4               │  ② 音频主引擎
   │  I2S Slave RX → FX → 鼓机混音   │
   │  + Melody loop + UI 音效        │
   │  → ES8311 codec → 板载 Speaker  │
   │  MIPI 屏 / 相机 / LVGL           │
   └──────────┬─────────────────────┘
              │ SDIO (ESP-Hosted)
   ┌──────────▼──────────┐
   │ 板载 ESP32-C6       │  仅 Wi-Fi 协处理器
   └─────────────────────┘
```

## 业务分配

| 功能 | 芯片 | 说明 |
|------|------|------|
| A2DP 收歌 + SBC/AAC 解码 | ESP32 | 唯一支持 Classic BT |
| AVRC 元数据（歌名/歌手） | ESP32 → UART → P4 | 小数据，不走 I2S |
| **PCM 音频流传输** | ESP32 → **I2S** → P4 | 必须用 I2S，UART 带宽不够 |
| 实时 FX（HP/LP/Gate/Delay/Stutter） | **P4** | 算力充裕，与 UI/Melody 同芯片 |
| Drum 鼓垫合成 + 混音 | **P4** | 鼓垫触发经 UART 或 P4 本地按键/IMU |
| Melody loop 混音 | **P4** | 与 A2DP 在同一条混音总线 |
| **唯一喇叭输出** | **P4 → ES8311** | 板载 codec，可拆 MAX98357A |
| Wi-Fi / 网络 | P4（经 C6 Hosted） | 不动 C6 固件 |
| 大屏 UI / 相机 | P4 | MIPI-DSI + OV5647 |

## 为什么可行

P4 性能做 FX + 混音完全够用，`12_I2SCodec` 已验证 ES8311 播放链路。ESP32 退化为「蓝牙 Modem + I2S 发送端」，职责清晰。

## 关键约束

### 1. 必须用 I2S 传 PCM，不能用 UART 传音频

| 格式 | 带宽需求 | UART 921600 | I2S |
|------|----------|-------------|-----|
| 44.1kHz 立体声 16bit | ~1.4 Mbps | ❌ 不够 | ✅ |
| 44.1kHz 单声道 16bit | ~0.7 Mbps | ⚠️ 勉强 | ✅ |

建议：**ESP32 = I2S Master TX，P4 = I2S Slave RX**，3 线（BCLK、WS、DIN）+ 共地。采样率跟 A2DP 协商结果（通常 44100 或 48000）。

### 2. 延迟

- A2DP 本身已有 ~100–200 ms（手机端）
- ESP32→P4 I2S 再加 ~10–30 ms（取决于 buffer）
- 对 AirMix 手势 FX 通常可接受；P4 侧 audio task 优先级要设高

### 3. 时钟

ESP32 做 I2S Master，P4 做 Slave 跟时钟，避免两芯片各自跑采样率漂移。

### 4. P4 负载

P4 同时跑 LVGL/相机时，音频 task 需独立高优先级 + 固定 block size（如 128–256 samples），FX 先用 float，后期可定点优化。

## 分阶段落地

| 阶段 | ESP32 | P4 | 验证点 |
|------|-------|-----|--------|
| **A** | A2DP → I2S TX 直通 | I2S RX → ES8311 播放 | 手机蓝牙音乐从 P4 喇叭出来 |
| **B** | 同上 + UART 发元数据 | 搬 `BT_Player` 鼓机 + 混音 | 鼓垫有声 |
| **C** | 仅 PCM 桥 | 加 FX 链 + Melody 混音 | 完整 AirMix 音频 |
| **D** | 可选：鼓垫触发仍放 ESP32 或改 P4 本地 IMU | 统一混音引擎 | 产品级 |

## ESP32 侧要删/改什么

从现有 `BT_Player.c` 中：

- **保留**：A2DP/AVRC 初始化、`bt_player_a2dp_data_cb`、ringbuffer
- **改为**：I2S TX 指向 P4（不再接 MAX98357A）
- **移出到 P4**：`bt_player_mix_drums`、FX、I2S 最终 write 到 ES8311

## 物理接线（原型）

```
ESP32                    P4
GPIO27 BCLK  ──────────► I2S_BCLK (待选 GPIO，参考 P4 空闲脚)
GPIO14 WS    ──────────► I2S_WS
GPIO22 DOUT  ──────────► I2S_DIN
GND          ──────────► GND
GPIO? TX     ──────────► UART RX  （元数据 / 控制）
GPIO? RX     ◄────────── UART TX
```

P4 的 ES8311 仍走板载 I2S 到 codec；**ESP32→P4 是第二路 I2S 输入**，P4 软件混音后再写 ES8311。

---

## Workspace

| 文件夹 | 芯片 | 路径 |
|--------|------|------|
| ① ESP32 | esp32 | `projects/esp32-a2dp` |
| ② ESP32-P4 | esp32p4 | `projects/esp32p4-wifi` |
| ③ 算法训练 | PC | `algorithm-training` |

P4 音频：`hardware/.../12_I2SCodec`（ES8311 BSP）
