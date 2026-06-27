# ESP32-P4 → ST7789V 240×320 SPI 接线

模块引脚：**VCC, GND, DIN, CLK, CS, DC, RST, BL**

> 你的 **ESP32-P4-WIFI6** 扩展排针丝印是 **`GPIO20`、`GPIO5`** 这种格式，**没有 GPIO6**。  
> **DC 请接 GPIO26**（固件已改）。

## 接线表（按你板子丝印）

| 屏模块 | 接到 P4 排针丝印 | 说明 |
|--------|------------------|------|
| **VCC** | **3V3** | 右侧排针，勿接 5V/VBUS |
| **GND** | **GND** | 任选一个 GND |
| **DIN** | **GPIO20** | 右侧，SPI MOSI |
| **CLK** | **GPIO21** | 右侧，紧挨 GPIO20 |
| **CS** | **GPIO5** | 左侧 |
| **DC** | **GPIO26** | 右侧（GPIO23 下面） |
| **RST** | **GPIO22** | 右侧 |
| **BL** | **GPIO23** 或 **3V3** | 背光；可直连 3V3 常亮 |

### 右侧排针（USB 下方，从上到下）

```
3V3 → GPIO20(DIN) → GPIO21(CLK) → GND → GPIO22(RST) → GPIO23(BL) → RUN → GPIO26(DC) → …
```

### 左侧

```
… → GPIO5(CS) → GPIO4 → GND → GPIO3 → GPIO2 → …
```

## 不要占用的脚

| 丝印 | 用途 |
|------|------|
| GPIO2, 3, 4 | ESP32 → P4 I2S 音频 |
| GPIO7, 8 | ES8311 I2C (SDA/SCL) |
| GPIO24, 25 | USB D-/D+ |

## 刷机

workspace **② ESP32-P4** → Build / Flash。串口应出现：

```text
ST7789V 240x320 SPI MOSI=20 SCLK=21 CS=5 DC=26 RST=22 BL=23
```

## 显示异常

改 `firmware/esp32p4-bt-bridge-rx/main/lcd_st7789_pins.h` 里 `LCD_GAP_*`、`LCD_MIRROR_*`。

---

## 已验证颜色配置（ColorChecker 2026-06）

在 **ST7789V 240×320 SPI**（本工程接线）上，用 X-Rite ColorChecker Classic 6×4 色卡 **逐块核对通过** 的配置如下。**后续摄像头预览、LVGL UI 均沿用此 LCD 设置，勿改回旧方案。**

| 项目 | 值 | 代码位置 |
|------|-----|----------|
| RGB 通道顺序 | **RGB** | `lcd_st7789_pins.h` → `LCD_RGB_ELEMENT_ORDER_RGB` |
| 字节序 | **小端 LE** | `lcd_st7789.c` → `data_endian = LCD_RGB_DATA_ENDIAN_LITTLE` |
| 面板反色 | **开启** | `lcd_st7789_pins.h` → `LCD_PANEL_INVERT_COLOR true`；`lcd_st7789_init()` 内 `esp_lcd_panel_invert_color()` |
| 镜像 | X/Y 均为 **true** | `lcd_st7789_pins.h` → `LCD_MIRROR_X/Y` |
| SPI 像素时钟 | 40 MHz | `LCD_PIXEL_CLOCK_HZ` |

### 送屏数据路径（摄像头预览）

```
OV5647 → esp_video (RGB565) → PPA 缩放 → esp_cache_msync(C2M) → esp_lcd_panel_draw_bitmap
```

- **不做** 软件 `~` 全帧反相、**不做** `rgb565_swap_bytes`、**不做** 逐像素 luma/invert hack  
- PPA：`rgb_swap = false`，`byte_swap = false`  
- SPI 双缓冲 + `on_color_trans_done` 等待（防撕裂）

### 已废弃、勿再使用的组合

| 组合 | 现象 |
|------|------|
| BGR + invert off | 摄像头红绿尚可，**黑白反** |
| BGR + invert on | 黑白对，**红蓝互换** |
| invert off + 送屏前软件 `~` | 灰度行对，**彩色色相乱** |
| 每帧 CPU 改 PPA buffer | 花屏/频闪（cache 未 M2C invalidate） |

### 模式切换（menuconfig / sdkconfig）

| 模式 | Kconfig |
|------|---------|
| 色卡自检（无摄像头） | `CONFIG_AIRMIX_LCD_COLORCHECKER=y` |
| OV5647 摄像头预览 | `CONFIG_AIRMIX_CAMERA_PREVIEW=y` |
| 蓝牙音频桥 + LVGL | `CONFIG_AIRMIX_BT_BRIDGE=y` |

色卡测试代码：`main/lcd_colorchecker.c`（白块黑 **B**、黑块白 **W** 标记）。

## 参考

- [esp32_p4_i2s_wiring.md](esp32_p4_i2s_wiring.md)

---

## 蓝牙元数据 UART（ESP32 → P4 屏幕）

除 I2S 音频外，再加 **1 根信号线**：

| 信号 | ESP32 | P4 丝印 |
|------|-------|---------|
| TX → RX | **GPIO 17** | **GPIO27** |
| GND | GND | GND |

115200 8N1。ESP32 在连接/播放/切歌时发送歌名、艺术家到 P4 屏幕。

两板都需刷最新固件（ESP32 + P4）。
