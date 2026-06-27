# Cursor / ESP-IDF 开发环境记忆（AirMix 双板）

> 最后更新：2026-06-26  
> 适用板子：ESP32 Classic（A2DP 桥）+ ESP32-P4 v1.3（Waveshare P4-WIFI6）

---

## 1. 打开方式

**必须**用 multi-root workspace 打开，不要单独打开子文件夹：

```
AirMix/AirMix.code-workspace
```

| 文件夹 | 芯片 | 固件路径 |
|--------|------|----------|
| ① ESP32 (A2DP / 音频) | ESP32 Classic | `projects/esp32-a2dp` |
| ② ESP32-P4 (播放 ESP32 蓝牙) | ESP32-P4 v1.3 | `firmware/esp32p4-bt-bridge-rx` |

操作 build / flash / monitor 前，先在左侧选中对应文件夹并打开该目录下的文件。底部状态栏的 target、port 跟着**当前激活文件夹**走。

---

## 2. 串口对照（macOS）

| 板子 | 串口 | 识别特征 |
|------|------|----------|
| ESP32 Classic | `/dev/cu.usbserial-110` | USB-UART 桥 |
| ESP32-P4 | `/dev/cu.usbmodem5B5E1340521` | 原生 USB（号码随插拔可能变） |

查当前端口：

```bash
ls /dev/cu.usb*
```

**刷机 / monitor 用 `cu.*`，不要用 `tty.*`**（macOS 上 `tty.*` 可能导致 gdb 挂起）。

---

## 3. ESP32-P4 v1.3 芯片版本（关键）

本板是 **P4 rev v1.3**，不是 v3.x。Device Target 只能选 `esp32p4`，**没有单独的 v1.3 选项**；版本靠 sdkconfig defaults 锁定。

### 持久配置文件

```
firmware/esp32p4-bt-bridge-rx/sdkconfig.defaults
firmware/esp32p4-bt-bridge-rx/sdkconfig.defaults.pre_v3
```

必须包含：

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_1=y
CONFIG_PARTITION_TABLE_OFFSET=0x9000
```

### Cursor / VS Code 设置（已写入）

- `AirMix.code-workspace` → ② ESP32-P4 文件夹 settings
- `firmware/esp32p4-bt-bridge-rx/.vscode/settings.json`

关键项：

```json
"idf.adapterTargetName": "esp32p4",
"idf.buildPath": "${workspaceFolder}/build-esp32p4",
"idf.port": "/dev/cu.usbmodem5B5E1340521",
"idf.sdkconfigDefaults": [
  "sdkconfig.defaults",
  "sdkconfig.defaults.pre_v3"
],
"idf.customExtraVars": {
  "IDF_TARGET": "esp32p4"
}
```

修改 settings 后：`Cmd+Shift+P` → **Developer: Reload Window**

### 验证 v1.3 没被重置

```bash
grep ESP32P4_SELECTS_REV firmware/esp32p4-bt-bridge-rx/sdkconfig
```

应看到 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`。

若被重置为 3.x：

```bash
cd firmware/esp32p4-bt-bridge-rx
rm -f sdkconfig
source $IDF_PATH/export.sh
idf.py set-target esp32p4
idf.py -B build-esp32p4 fullclean build
```

正确 bootloader 大小约 **0x5e40**（v1.x）；若是 0x6030 且要求 v3.1+，说明又编成了 3.x。

---

## 4. Build 目录

P4 项目 build 输出在 **`build-esp32p4/`**，不是默认的 `build/`：

```
build-esp32p4/esp32p4-bt-bridge-rx.elf   ← 正确
build/esp32p4-bt-bridge-rx.elf           ← 不存在，Cursor 未 Reload 时会找错
```

命令行始终加 `-B build-esp32p4`：

```bash
cd firmware/esp32p4-bt-bridge-rx
source $IDF_PATH/export.sh
idf.py -B build-esp32p4 -p /dev/cu.usbmodem5B5E1340521 flash monitor
```

---

## 5. 串口无法访问（Resource temporarily unavailable）

### 现象

```
[Errno 35] Could not exclusively lock port /dev/cu.usbmodem5B5E1340521
ELF file '.../build/esp32p4-bt-bridge-rx.elf' does not exist
```

### 原因 A：端口被占用

另一个 monitor / flash / Cursor 串口面板占着 P4 口。查占用：

```bash
lsof /dev/cu.usbmodem5B5E1340521
```

结束占用进程（把 PID 换成 lsof 输出的数字）：

```bash
kill <PID>
```

或：关掉所有 Cursor 终端里的 monitor（`Ctrl+]`）、ESP-IDF Monitor 面板、Serial Monitor 扩展。

### 原因 B：ELF 路径错误

扩展还在找 `build/` 而非 `build-esp32p4/` → **Reload Window**，或在 ② 文件夹重新 Build 一次。

---

## 6. 常用命令速查

### ESP32-P4

```bash
cd /Users/ziyiyang/Desktop/AirMix/firmware/esp32p4-bt-bridge-rx
source /Users/ziyiyang/espressif/v6.0.1/esp-idf/export.sh
unset IDF_TARGET
idf.py set-target esp32p4
idf.py -B build-esp32p4 -p /dev/cu.usbmodem5B5E1340521 build flash monitor
```

### ESP32 Classic

```bash
cd /Users/ziyiyang/Desktop/AirMix/projects/esp32-a2dp   # 或实际 ESP32 工程路径
source /Users/ziyiyang/espressif/v6.0.1/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-110 build flash monitor
```

---

## 7. I2S 硬件连接（ESP32 → P4）

| 信号 | ESP32 GPIO | P4 GPIO |
|------|------------|---------|
| BCLK | 27 | 2 |
| WS/LRC | 14 | 3 |
| DOUT → DIN | 22 | 4 |
| GND | GND | GND |

ESP32 侧 `BT_PLAYER_OUTPUT_P4=1`，不接 MAX98357A。P4 用板载 ES8311 喇叭输出。

详见 `docs/esp32_p4_i2s_wiring.md`。

---

## 8. 刷机成功后的 P4 日志

```
=== AirMix P4 BT bridge RX ===
I2S0 slave RX: BCLK=GPIO2 WS=GPIO3 DIN=GPIO4 @ 44100 Hz
```

若只有 ROM 启动信息、没有 `=== AirMix P4 BT bridge RX ===`：

**根因（已验证）：控制台配置错误。** Waveshare P4 板日志走 **UART GPIO 37/38**，不能只配 USB 控制台。

`sdkconfig.defaults` 必须是：
```
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_NUM=0
CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y
```

修改后执行 `rm -f sdkconfig && idf.py fullclean build flash monitor`。

正常启动还会看到：`GPIO 38 and 37 are used as console UART I/O pins`

其他：先开 monitor 再按 RESET；用 Ctrl+] 避免 port busy。

---

## 10. Waveshare 平台 Build/Run 说明（与 AirMix 的关系）

官方 Platform 文档里的通用注意事项，对照本原型：

| 官方说明 | AirMix 现状 |
|----------|-------------|
| P4 **无内置 Wi-Fi / 蓝牙** | ✅ 设计如此：蓝牙在 **ESP32 Classic**；Wi-Fi 以后走板载 **C6 + ESP-Hosted**，当前阶段不碰 C6 固件 |
| Wi-Fi 例程需 C6 等协处理器 | ⏸ 暂缓；P4 侧以后用 `10_wifistation` + Hosted，不是现在 |
| 以太网需 PHY / menuconfig 改 GPIO | ❌ 当前不用 |
| **Core-DEV-KIT 兼容矩阵不含 I2S Codec** | ⚠ 文档指最小核心板；你的是 **P4-WIFI6 整板 + ES8311**，`12_I2SCodec` 路线正确，我们已跑通 codec |
| 显示 / LVGL / Brookesia 需 PSRAM | ✅ 已启用 PSRAM；以后做 UI 再开 LVGL，现在只做音频桥 |
| 先跑 `00_board_check` 确认 PSRAM | ✅ 等价验证：HelloWorld / 当前固件日志里有 `esp_psram: SPI SRAM memory test OK` |
| `idf_component.yml` 下载 managed 组件可能卡住 | 网络 / 代理问题；本项目依赖 `waveshare/esp32_p4_platform` |
| GPIO 例程可能与板载外设 **引脚冲突** | ✅ 外部 I2S 用 **GPIO 2/3/4**；勿用 7–13（ES8311）、14–19（C6 SDIO）、53/54 |

### 本板已验证可用的外设（当前阶段）

- USB 刷机 + monitor（UART 主控 + USB 副控）
- PSRAM 32MB @ 200MHz
- ES8311 喇叭（I2S1 / BSP）
- 外部 I2S Slave RX（I2S0 / GPIO 2/3/4 ← ESP32）

### 本阶段刻意不用的例程

SDMMC、Ethernet、eth2ap、Wi-Fi station、摄像头、Brookesia 手机 UI — 与「ESP32 蓝牙 → P4 播放」原型无关，避免占引脚、增复杂度。

---

## 11. IDF 环境

- IDF 路径：`/Users/ziyiyang/espressif/v6.0.1/esp-idf`
- 每个新终端需：`source $IDF_PATH/export.sh`
- P4 工具链：`python $IDF_PATH/tools/idf_tools.py install --targets esp32p4`
