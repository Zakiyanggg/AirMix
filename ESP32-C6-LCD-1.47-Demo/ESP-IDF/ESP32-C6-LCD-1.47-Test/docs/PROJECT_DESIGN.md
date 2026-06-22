# ESP32 手持音乐设备 — 项目设计总结

> 文档整理日期：2026-06-10  
> 涵盖：产品愿景、功能模块、硬件架构建议、Melody 设备端方案、开发路线与当前工程状态。

---

## 1. 产品愿景

一款**手持音乐创作设备**，用户无需依赖手机即可完成核心玩法（Melody 模式目标为全设备端闭环）。

### 四大核心模式

| 模式 | 描述 |
|------|------|
| **Bluetooth** | 连接手机播放蓝牙音乐（A2DP），显示歌名/歌手/波形 |
| **Drum** | 手持时通过 4 个物理按键实时触发鼓点，混入蓝牙 PCM 流 |
| **FX** | 通过陀螺仪/加速度计体感控制效果器强弱（HP/LP/Reverb/Echo/Flanger 等） |
| **Melody** | 拍摄一张照片作为「专辑封面」→ 分析画面氛围与情绪 → 生成匹配 Loop → 用户按键在和弦进行内即兴创作 → 保存与分享 |

### 物理交互

- **4 物理按键**（K1–K4）：低电平有效，映射 SN / BD / OH / CH（鼓模式）或音阶音符（Melody 模式）
- **陀螺仪**：Pitch/Roll/Yaw 映射 FX 参数
- **屏幕**：横屏 UI，展示当前模式、参数、封面缩略图等

### 设计原则

- Melody 的「拍照定调」不依赖识别具体物体，而是读取**色彩、亮度、色温、纹理**等视觉特征，映射到音乐参数
- 设备端不做大模型图像理解或神经网络作曲；采用**轻量视觉分析 + 规则/小模型 + SD 采样库 + 程序化合成**
- 蓝牙音乐（A2DP）与 Melody 可分时运行或模式切换，避免单芯片内存过载

---

## 2. 当前工程状态

### 目标芯片（当前编译）

- **ESP32（Classic，Xtensa 双核 240MHz）**
- Flash 4MB，无 PSRAM
- `sdkconfig` 中 `CONFIG_IDF_TARGET=esp32`

### 已实现功能

| 模块 | 路径 / 说明 |
|------|-------------|
| TFT + LVGL UI | `main/TFT_Test/` — ST7735 **160×128** 横屏 |
| 蓝牙 A2DP + AVRCP | `main/BT_Player/` — 歌名/歌手元数据 |
| 鼓声实时合成混音 | `BT_Player_TriggerDrum()` 混入蓝牙 PCM |
| UI 界面 | `main/LVGL_UI/` — Bluetooth / FreePlay 布局，`ui_common.h` 颜色体系 |
| 中文歌名字库 | `ui_font_zh_12.c`（约 1145 常用汉字） |
| PC 端 LCD 模拟器 | `tools/lcd_sim/` — Pygame 160×128 预览，键盘 1–4 模拟物理键 |

### 关键引脚（当前 ESP32 Demo）

| 功能 | GPIO |
|------|------|
| TFT SPI | SCLK=18, MOSI=23, CS=5, DC=16, RST=17, BLK=4 |
| I2S → MAX98357A | BCLK=27, LRC=14, DIN=22, AMP_SD=21 |
| 按键 K1–K4 | 32, 33, 25, 26 |

### 已知约束

- BT 栈保留约 **56KB DRAM**；48KB 音频 ringbuffer 已接近内存上限
- 无 PSRAM，无法承载摄像头帧缓冲
- **ESP32-C3 / C6 / S3 均不支持 Classic Bluetooth（A2DP）**，不能替代 ESP32 做蓝牙音箱

### 备用 / 参考代码

- **ESP32-C6 1.47 寸屏工程**：`main/backup_c6_test/` — ST7789 **172×320**、SD 卡、MPU6050
- **C6 按键 + IMU 示例**：`main/LVGL_UI/LVGL_Example.c` — GPIO 0–3 按键，I2C MPU6050

---

## 3. 硬件架构建议

### 3.1 核心结论：多芯片分工，而非单芯片包办

| 芯片 | 角色 | 原因 |
|------|------|------|
| **ESP32（Classic）** | 音频主控：A2DP、I2S、PCM 混音、FX DSP | 唯一支持 Bluetooth Classic / A2DP |
| **ESP32-S3（N16R8，8MB PSRAM）** | 视觉 + Melody：摄像头、情绪分析、Loop 引擎、SD 采样 | PSRAM、AI 指令、esp_camera 生态 |
| **ESP32-C3 + LCD** | 交互主控：高分辨率 UI、按键、IMU、菜单 | GPIO 简单，不占 BT/相机内存 |

```
        （可选）手机 — 仅用于分享/OTA，非 Melody 必需
                    │
    ┌───────────────┴───────────────────────────────────┐
    │                  手持设备 PCB                      │
    │  ┌─────────────┐  UART   ┌─────────────────────┐  │
    │  │ ESP32-C3    │◄──────►│ ESP32-S3 + Camera   │  │
    │  │ 屏幕/按键/IMU│        │ 拍照/分析/Loop/SD   │  │
    │  └─────────────┘        └──────────┬──────────┘  │
    │                                    │ UART/I2S    │
    │                         ┌──────────▼──────────┐  │
    │                         │ ESP32 (Audio)       │  │
    │                         │ A2DP + 混音 + FX    │  │
    │                         │ I2S → MAX98357A     │  │
    │                         └─────────────────────┘  │
    └───────────────────────────────────────────────────┘
```

### 3.2 现有两块开发板的定位

| 板子 | 建议角色 |
|------|----------|
| **原版 ESP32（当前 Demo）** | 音频引擎，继续迭代 BT + 鼓机 + FX |
| **ESP32-C3 + 更高像素屏** | UI / 菜单 / 体感交互；**不**承担 A2DP 或拍照分析 |

### 3.3 推荐原型 BOM

| # | 器件 | 说明 |
|---|------|------|
| 1 | ESP32-WROOM-32E（4MB Flash） | 音频主控 |
| 2 | ESP32-S3-WROOM-1 **N16R8** | 8MB PSRAM，Melody 核心 |
| 3 | ESP32-C3 + LCD 一体板 | UI 主控 |
| 4 | 摄像头 **OV2640** | 2MP，esp_camera 支持成熟 |
| 5 | MAX98357A I2S 功放 | 现有方案 |
| 6 | MPU6050 / QMI8658 | 体感 FX（原型可用 MPU6050） |
| 7 | microSD（SPI，8–16GB） | Loop 库、封面、录音 |
| 8 | ST7789 屏（172×320 或 240×320） | 比 160×128 信息量大，工程有 ST7789 驱动可参考 |
| 9 | 4× tactile 按键、3.7V 锂电 | 手持形态 |

### 3.4 屏幕选型

| 屏幕 | 分辨率 | 建议 |
|------|--------|------|
| 当前 ST7735 1.8" | 160×128 | 功能验证，信息偏挤 |
| C3/C6 常见 1.47" ST7789 | 172×320 | **原型 UI 推荐**，横屏 320×172 |
| 2.0" ST7789 | 240×320 | 量产升级方向 |

UI 设计基准可从 160×128 迁移至 320×172；PC 模拟器 `tools/lcd_sim/` 可同步改分辨率。

---

## 4. 各功能模块设计

### 4.1 Bluetooth + Drum（已基本可用）

**数据流：**

```
手机 A2DP → ESP32 解码 → ringbuffer → 鼓声合成混音 → I2S → 喇叭
                ↑
         K1–K4 触发 BT_Player_TriggerDrum()
```

**后续：** 菜单切换、组合键、与 UI 任务解耦优化内存。

### 4.2 FX 体感控制

**硬件：** I2C IMU（MPU6050 原型 → BMI270 量产）

**映射建议：**

| 体感 | FX 参数 |
|------|---------|
| Pitch（前后倾） | LP / HP cutoff |
| Roll（左右倾） | Reverb mix / Echo delay |
| Yaw / 甩动 | Flanger depth |
| 静止 | 参数缓慢回归默认 |

**实现：** IMU 可在 C3 读取，FX DSP 在 ESP32 Audio 的 PCM 链上执行。

**算力现实：**

- HP/LP（IIR）：无压力
- Echo：短 delay line，注意 DRAM
- Reverb：简化 FDN，避免大卷积
- Flanger：短 delay + LFO，可行

### 4.3 Melody 模式（设备端完整闭环）

#### 用户流程

1. 组合键进入 Melody 模式  
2. 摄像头预览 → 用户拍照（任意照片作封面）  
3. 设备分析氛围（「Analyzing…」）  
4. 显示 mood 标签与 Loop 参数  
5. 背景 Loop 播放，4 键在当前和弦音阶内演奏  
6. 录制 → 保存至 SD / 后续分享  

#### 设备端流水线

```
[OV2640] → 抓拍 QVGA/VGA
    ↓
[视觉特征] warmth, saturation, brightness, contrast, complexity, color_spread
    ↓  （可选 96×96 tiny CNN 粗场景分类）
[情绪向量] energy, valence, tension, space … (0–100)
    ↓
[音乐参数] BPM, key, scale, chord_prog, drum_pattern, bass_style, fx_preset
    ↓
[Loop 引擎] SD 采样切片 + 程序化拼接 + 实时合成
    ↓
[4 键演奏] 五声/七声音阶映射
    ↓
[SD 保存] covers/ + recordings/
```

#### 能力边界

| 能做 | 不能做 |
|------|--------|
| 色彩/氛围 → 情绪标签 | 深度语义理解（「这是猫」） |
| 参数化 Loop 生成 | 神经网络端到端作曲 |
| 本地录制与回放 | 不依赖手机的完整创作闭环 |

#### 核心数据结构（设计草案）

```c
typedef struct {
    float warmth, saturation, brightness, contrast, complexity, color_spread;
} photo_features_t;

typedef struct {
    float energy, valence, tension, space;  /* 0–100 */
} mood_t;

typedef struct {
    uint8_t bpm, key, scale;
    uint8_t chord_prog[4];
    uint8_t drum_pattern, bass_style, pad_style, fx_preset;
} loop_params_t;
```

#### SD 卡目录建议

```
/sd/
  loops/
    warm_calm/      bass_*.wav  drum_*.wav  pad_*.wav
    cold_dark/
    bright_energy/
  covers/           用户封面 JPEG 缩略图
  recordings/       用户作品
  presets/          和弦/映射 JSON
```

---

## 5. 开发路线

### 阶段 A — 单 ESP32 验证核心玩法（当前）

- [x] A2DP + I2S + 鼓机混音  
- [x] 160×128 LVGL UI（Bluetooth）  
- [x] PC LCD 模拟器  
- [ ] 升级 ST7789 高分辨率屏  
- [ ] 接入 IMU + 1–2 个 FX  
- [ ] 菜单 / 组合键逻辑（模拟器 + 设备同步）  

### 阶段 B — ESP32-S3 验证 Melody 链路（无需手机）

- [ ] OV2640 拍照 + 预览  
- [ ] 视觉特征 → mood 映射（PC 算法移植轻量版）  
- [ ] SD Loop 库 + 播放引擎  
- [ ] 4 键音阶演奏 + 录制  

### 阶段 C — 三芯片整合

- [ ] C3 UI ↔ S3 Melody ↔ ESP32 Audio UART 协议  
- [ ] 模式切换：Bluetooth / Drum / FX / Melody  
- [ ] 外壳与手持形态原型机  

### 阶段 D — 产品化

- [ ] 采样库扩充、算法调参  
- [ ] 分享/export（WiFi 或 USB，非 Melody 必需路径）  
- [ ] 量产 BOM 与 PCB  

---

## 6. PC 端 LCD 模拟器

**路径：** `tools/lcd_sim/`

**启动：**

```bash
cd tools/lcd_sim
./run.sh
```

**功能：** 160×128 横屏、Bluetooth / FreePlay 两屏、键盘 1–4 模拟 K1–K4。

**后续扩展：** 菜单导航、组合键、与设备串口同步、分辨率升级至 320×172。

---

## 7. 关键决策备忘

| 问题 | 决策 |
|------|------|
| 能否只用 ESP32-C3 替代 ESP32？ | **不能**（无 A2DP） |
| Melody 是否必须带手机？ | **不必**；需 S3 + 摄像头 + SD |
| 情绪分析放哪里？ | **ESP32-S3**；C3 算力与内存不足 |
| 图像「理解」程度？ | 氛围/特征级，非物体识别 |
| Loop「生成」含义？ | SD 选片 + 参数化拼接 + 合成，非 AI 作曲 |
| 原型最快路径？ | 现有 ESP32 继续 BT+鼓；S3 单独验证 Melody；再整合 |

---

## 8. 相关源码索引

| 内容 | 路径 |
|------|------|
| 主入口 | `main/main.c` |
| TFT / 按键 | `main/TFT_Test/TFT_Test.h`, `TFT_Test.c` |
| 蓝牙 + 鼓机 | `main/BT_Player/BT_Player.c`, `BT_Player.h` |
| UI 组件 | `main/LVGL_UI/ui_bluetooth.c`, `ui_common.h` |
| C6 屏 + SD + IMU 参考 | `main/backup_c6_test/`, `LVGL_Example.c` |
| ST7789 驱动 | `main/LCD_Driver/ST7789.h` |
| LCD 模拟器 | `tools/lcd_sim/main.py` |
| UI 设计稿 | `UI_02_Bluetooth_160x128.svg`, `UI_01_FreePlay_160x128.svg` |

---

## 9. 待办与开放问题

- [ ] 三芯片 UART 协议详细定义（C3 ↔ S3 ↔ ESP32 Audio）  
- [ ] `photo_features` → `mood` → `loop_params` 映射表与 PC 调参工具  
- [ ] FreePlay 屏在固件中与 Bluetooth 屏切换  
- [ ] Melody 与 A2DP 同时运行 vs 互斥模式切换策略  
- [ ] 双 ESP32 共喇叭方案（若阶段性只用两颗 ESP32）  

---

*本文档随项目演进更新；硬件与算法有重大变更时请同步修订。*
