# AirMix 项目结构

整体仓库名：**AirMix**（手持互动音乐设备）

当前分为 **3 个可独立开发的子项目**，在 Cursor 左下角切换 workspace 文件夹即可对应不同 build / 刷机目标。

---

## 总览

```
AirMix/                                    ← 仓库根目录
├── AirMix.code-workspace                  ← 用此文件打开整个工作区
├── docs/                                  ← 产品 / 架构文档（共享）
│
├── [子项目 1] tools/lcd-sim               ← PC 屏幕 UI 模拟（Pygame）
│   └── run.sh                             ← 320×172 UI 预览，无需板子
│
├── [子项目 2] firmware/esp32-bt-audio     ← ESP32 经典款 · 蓝牙音乐
│   ├── main/BT_Player/                    ← A2DP 蓝牙播放 + 鼓点混音
│   ├── main/TFT_Test/                     ← ST7735 160×128 横屏
│   ├── main/LVGL_UI/                      ← UI 设计稿（含 320×172 SVG）
│   └── sdkconfig                          ← CONFIG_IDF_TARGET=esp32
│
├── [子项目 3] firmware/esp32c6-display      ← ESP32-C6 · 1.47 寸屏显示
│   ├── main/LCD_Driver/                   ← ST7789 172×320
│   ├── main/LVGL_UI/                      ← LVGL 界面
│   ├── main/backup_c6_test/               ← C6 原始入口参考
│   └── sdkconfig                          ← 目标 esp32c6（待 set-target）
│
├── desktop-prototype/  （可选）           ← Mac 算法模拟 airmix_demo
│   └── src/ + CMakeLists.txt              ← 当前仍在仓库根目录
│
└── hardware/reference-board/              ← 厂商 Demo 参考（Arduino 等）
    └── ESP32-C6-LCD-1.47-Demo/
```

> **说明**：子项目 2、3 目前物理上仍共用同一份 ESP-IDF 工程源码（历史路径 `ESP32-C6-LCD-1.47-Demo/...`）。workspace 里已拆成两个逻辑文件夹，并分别配置 **esp32** / **esp32c6** 刷机目标。后续可再物理拆成两个独立 `firmware/` 目录。

---

## 三个子项目对照

| # | Workspace 名称 | 芯片 | 主要功能 | Build | Flash |
|---|----------------|------|----------|-------|-------|
| 1 | **LCD 屏幕模拟** | 无（Mac） | 320×172 UI 预览 | `./run.sh` | 不需要 |
| 2 | **ESP32 蓝牙音乐** | ESP32 Classic | A2DP 播歌 + 鼓机 + 160×128 UI | ESP-IDF Build | ESP32 板 |
| 3 | **ESP32-C6 屏幕显示** | ESP32-C6 | 1.47 寸 ST7789 + LVGL UI | ESP-IDF Build | C6 板 |

---

## 左下角切换文件夹后怎么操作

### 1 · LCD 屏幕模拟

```bash
cd tools/lcd-sim   # 或完整路径见 workspace
./run.sh
```

- **Tab** 切换 Bluetooth / FreePlay
- **1–4** 模拟物理键
- 设计稿：`firmware/esp32-bt-audio/main/LVGL_UI/UI_*_320x172.svg`

### 2 · ESP32 蓝牙音乐（刷 classic ESP32）

1. 左下角选 **ESP32 蓝牙音乐**
2. `Cmd+Shift+P` → **ESP-IDF: Pick a Workspace Folder** → 选同一文件夹
3. 底部状态栏确认芯片为 **esp32**
4. 点 🔥 **Build** → **Flash**

首次或换板后：

```bash
cd firmware/esp32-bt-audio
idf.py set-target esp32
idf.py build flash monitor
```

### 3 · ESP32-C6 屏幕显示（刷 C6 板）

1. 左下角选 **ESP32-C6 屏幕显示**
2. 确认芯片为 **esp32c6**
3. Build → Flash

```bash
cd firmware/esp32c6-display
idf.py set-target esp32c6
idf.py build flash monitor
```

> C6 **不支持** Classic Bluetooth（A2DP），此子项目只做屏幕 / LVGL / IMU，不含蓝牙播歌。

---

## 第四个（可选）：桌面算法原型

路径：仓库根目录 `src/` + `CMakeLists.txt`

```bash
cmake -G Ninja -S . -B build && cmake --build build
./build/airmix_demo
```

终端文字输出，验证手势→音效算法，与 LCD 模拟器、固件是不同程序。

---

## 芯片选型备忘

| 能力 | ESP32 Classic | ESP32-C6 |
|------|---------------|----------|
| 蓝牙 A2DP 播歌 | ✅ | ❌ |
| 高分辨率 LCD | 160×128 Demo | ✅ 172×320 |
| 当前 AirMix 角色 | 音频主控 | UI / 显示 |

详见 `firmware/esp32-bt-audio/docs/PROJECT_DESIGN.md`。
