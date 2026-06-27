# ESP32-P4-WIFI6 本地开发资料

本目录保存 Waveshare ESP32-P4-WIFI6 开发板、ESP32-P4、OV5647 摄像头和 ESP-Hosted P4+C6 双芯片开发所需的本地资料。

## 已下载内容

```text
hardware/ESP32-P4-WIFI6/
├── camera/
│   └── RPi_Camera_B_OV5647_Wiki.html
├── esp-hosted/
│   ├── esp32_p4_function_ev_board.md
│   ├── esp_hosted_component_readme.html
│   └── esp_hosted_slave_example.html
├── examples/
│   └── ESP32-P4-Platform/
├── pdfs/
│   ├── ESP32-P4-WIFI6-schematic-design-file.pdf
│   ├── esp32-p4_datasheet_en.pdf
│   └── esp32-p4_technical_reference_manual_en.pdf
└── waveshare-pages/
    ├── ESP32-P4-WIFI6.html
    ├── Development-Environment-Setup-IDF.html
    └── Resources-And-Documents.html
```

## 核心架构

这块板不是单芯片 Wi-Fi MCU，而是双芯片结构：

- **ESP32-P4**：主控。负责应用逻辑、摄像头 MIPI-CSI、屏幕 MIPI-DSI、LVGL/UI、PPA 图像处理、I2S audio、SD 卡、USB 等高性能多媒体任务。
- **ESP32-C6**：无线协处理器。通过 **SDIO** 连接到 P4，为 P4 提供 Wi-Fi 6 / BLE 5 能力。

ESP32-P4 本身没有内置 Wi-Fi/Bluetooth。正常开发时，你主要写 **P4 固件**；C6 保持运行 **ESP-Hosted slave 固件**，让 P4 通过 `esp_wifi_remote` / `esp_hosted` 像使用本地 Wi-Fi 一样调用网络功能。

## 推荐开发环境

Waveshare 文档建议 ESP-IDF v5.4 或更新版本，推荐 v5.5.x。当前本机 shell 里没有 `idf.py`，说明 ESP-IDF 还没安装，或者安装后还没有 source 环境脚本。

macOS 常见流程：

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32p4,esp32c6
. ./export.sh
idf.py --version
```

以后每次打开新终端，都需要：

```bash
. ~/esp/esp-idf/export.sh
```

## P4 固件如何烧录

P4 是你的主应用固件目标：

```bash
cd hardware/ESP32-P4-WIFI6/examples/ESP32-P4-Platform/examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

macOS 上可以先找串口：

```bash
ls /dev/cu.*
```

如果自动进下载模式失败，手动进入 P4 bootloader：

1. 按住板子的 `BOOT`。
2. 按一下 `RESET/RST`。
3. 松开 `BOOT`。
4. 重新执行 `idf.py -p PORT flash monitor`。

如果遇到 chip revision 报错，不要用 `--force` 硬刷。先跑 `00_board_check` 看芯片版本，再参考示例仓库的 `docs/ESP32P4_REVISION_CONFIG.md`。生产版通常用 v3.x 配置；早期工程样片需要 pre-v3 overlay。

## C6 固件如何烧录

通常 **不需要先刷 C6**。C6 应该预装 ESP-Hosted slave 固件，P4 例程通过 SDIO 找到它。

只有这些情况才考虑刷 C6：

- P4 日志显示 SDIO/ESP-Hosted 一直连接不上。
- 你需要更新 ESP-Hosted slave 固件版本。
- 你明确要把 C6 当成独立 MCU 做自定义无线固件。

C6 是另一颗芯片，所以它需要 **单独的 ESP32-C6 工程**：

```bash
idf.py create-project-from-example "espressif/esp_hosted:slave"
cd slave
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p C6_UART_PORT flash monitor
```

C6 刷写需要接到板上的 **ESP32-C6 UART pads** 或等效下载接口。Waveshare 页面标出了 C6 UART Pads，具体 TX/RX/EN/BOOT/IO0 需要看 `pdfs/ESP32-P4-WIFI6-schematic-design-file.pdf` 和板子丝印。不要把 P4 的 USB-UART 口误认为一定能直接刷 C6。

重要提醒：如果你把 C6 刷成普通 `HelloWorld` 或自定义 BLE/Wi-Fi app，P4 侧的 `esp_wifi_remote` 会失效，因为 P4 期待 C6 跑的是 ESP-Hosted slave 协议。除非你要重写 P4-C6 通信协议，否则 C6 最好只跑 ESP-Hosted。

## P4 和 C6 如何分别开发功能

### P4 适合开发

- 摄像头采集：MIPI-CSI、OV5647、ISP、H.264/JPEG。
- 屏幕/UI：MIPI-DSI、LVGL、触摸屏、动画。
- 音频：I2S codec、麦克风、扬声器。
- 主应用：状态机、交互逻辑、图像/音频处理、文件系统、网络应用层。

目标命令：

```bash
idf.py set-target esp32p4
```

### C6 适合开发

- 作为 Wi-Fi/BLE 协处理器：ESP-Hosted slave。
- 非主线玩法：独立低功耗无线任务、自定义 BLE 广播、特殊无线控制。

目标命令：

```bash
idf.py set-target esp32c6
```

但 AirMix 这类产品建议第一阶段不要自定义 C6。先让 C6 做无线协处理器，把摄像头、UI、音频和主业务都放在 P4。

## 建议起步顺序

1. **确认板子和工具链**

```bash
cd hardware/ESP32-P4-WIFI6/examples/ESP32-P4-Platform/examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

看到 board check banner、PSRAM 信息、周期性 alive log 后再继续。

2. **确认屏幕链路**

```bash
cd ../13_Displaycolorbar
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

屏幕应显示色条。这个例程不涉及 LVGL 和摄像头，是最干净的显示 bring-up。

3. **确认 OV5647 摄像头到 LCD**

```bash
cd ../16_video_lcd_display
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

默认摄像头配置是 OV5647、MIPI RAW8、800 x 1280、50 FPS。默认 SCCB/I2C 为：

| Signal | GPIO |
| --- | --- |
| SCL | GPIO8 |
| SDA | GPIO7 |

如果画面打不开，优先检查：摄像头 FPC 方向、是否插到 MIPI-CSI 而不是 DSI、sensor 是否选择 OV5647、PSRAM 是否正常。

4. **确认 C6 Wi-Fi companion**

```bash
cd ../10_wifistation
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

在 `menuconfig` 里配置 Wi-Fi SSID/Password，并确认 Wi-Fi Remote / ESP-Hosted 选择的是 ESP32-C6 slave。这个例程的 `main/idf_component.yml` 已经引入 `esp_wifi_remote` 和 `esp_hosted`。

5. **摄像头网络流**

```bash
cd ../17_simple_video_server
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

串口里拿到 IP 后，在浏览器打开：

```text
http://esp-web.local
```

或者：

```text
http://<board-ip>
```

6. **确认音频 codec**

```bash
cd ../12_I2SCodec
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

默认非 BSP I2S/codec 参考引脚在例程 README 里；如果 Waveshare 板载 codec 与默认不同，以原理图和 BSP 为准。

## 对 AirMix 项目的建议

对你的手持互动音乐设备来说，P4+C6 这块板适合做更高级的原型：

- P4 跑 UI、摄像头、音频 DSP、IMU/按钮逻辑。
- C6 保持 ESP-Hosted，用来做 Wi-Fi/BLE 网络功能。
- 蓝牙音频 A2DP 不是 P4 的强项；如果目标仍是“手机蓝牙音乐输入”，需要确认 ESP-Hosted 对 Classic Bluetooth/A2DP 的支持路径，或者改成 Wi-Fi/USB/线性输入/外部蓝牙音频模块。
- 如果只是做摄像头 + 屏幕 + 音频交互，先别动 C6 固件，直接从 P4 例程组合功能。

最自然的开发路线是：`00_board_check` → `13_Displaycolorbar` → `16_video_lcd_display` → `10_wifistation` → `17_simple_video_server` → `12_I2SCodec` → 再把 AirMix 的动作/音频架构迁到 P4 ESP-IDF 工程里。

