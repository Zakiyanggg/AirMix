# AirMix

AirMix 是一个手持式互动音乐设备原型：手机把音乐通过蓝牙传入设备，设备作为音箱或 3.5mm 耳机输出端播放，同时根据用户的挥动、晃动、敲击、旋转、上抬、下压和按钮输入，对正在播放的音乐做实时音效处理。

当前仓库是第一版软件架构和可运行模拟原型。它不绑定任何具体芯片，目标是先把音频链路、动作识别、按钮控制、效果映射和 buffer 级 DSP 处理拆清楚，方便后续移植到 ESP32、STM32、Teensy、Raspberry Pi 或其他嵌入式平台。

## 如何运行 Demo

这是顶层 `AirMix` 的桌面模拟原型，不是 ESP-IDF 固件工程。

直接用系统 C++ 编译器：

```bash
c++ -std=c++17 -Isrc src/main.cpp -o build/airmix_demo
./build/airmix_demo
```

也可以用 CMake：

```bash
cmake -S . -B build
cmake --build build
./build/airmix_demo
ctest --test-dir build --output-on-failure
```

## ESP-IDF 固件工程

当前可烧录的 ESP-IDF 工程根目录是：

```text
ESP32-C6-LCD-1.47-Demo/ESP-IDF/ESP32-C6-LCD-1.47-Test
```

在 Cursor / VS Code 里构建固件时，需要打开这个目录，或者打开仓库根目录下的 `AirMix.code-workspace`，然后在 `ESP32 Audio Firmware` 这个 workspace folder 里执行 ESP-IDF build。不要把 `ESP32-C6-LCD-1.47-Demo` 当作 ESP-IDF 工程根目录；它上面没有固件工程的顶层 `CMakeLists.txt`。

迁移目录后，旧的 `build/` 目录里可能还缓存着原来的绝对路径，例如 `/Users/ziyiyang/Downloads/...`。第一次在新目录构建时建议重新生成 build：

```bash
cd ESP32-C6-LCD-1.47-Demo/ESP-IDF/ESP32-C6-LCD-1.47-Test
/Users/ziyiyang/espressif/v6.0.1/esp-idf/install.sh esp32
. /Users/ziyiyang/espressif/v6.0.1/esp-idf/export.sh
idf.py fullclean
idf.py set-target esp32
idf.py build
```

这份固件目前目标是 classic ESP32，不是 ESP32-C6；`sdkconfig` 中的目标为 `CONFIG_IDF_TARGET="esp32"`，因为 Bluetooth A2DP 需要 Classic Bluetooth。

## 当前已经实现

- `AudioInput` / `AudioOutput` 接口抽象，以及模拟音频输入输出。
- `IMUInput` / `ButtonInput` 接口抽象，以及模拟 IMU 和按钮序列。
- `MotionDetector` 的阈值式手势检测：shake、tilt left/right、punch/hit、rotate、lift/drop。
- `TapTempo`：用多次点击估计 BPM，供节奏效果使用。
- `GestureToEffectMapper`：把手势和按钮状态映射到 DSP 参数。
- `EffectProcessor`：按 audio buffer 应用 high-pass、low-pass、gate、delay、stutter、volume ducking。
- 三个轻量测试：motion detector、tap tempo、effect mapping。

## 当前是假数据 / 模拟

- 蓝牙音乐输入由 `SimulatedAudioInput` 生成正弦波代替。
- 扬声器 / 3.5mm 输出由 `SimulatedAudioOutput` 记录 RMS/peak 代替。
- IMU 数据由脚本化的 `SimulatedIMUInput` 代替。
- 实体按钮由脚本化的 `SimulatedButtonInput` 代替。
- DSP 算法是 MVP 级基础逻辑，优先验证交互链路，不追求最终音质。

## 后续如何接入真实硬件

- 用具体平台的蓝牙 A2DP sink 或 USB/line-in 驱动实现 `AudioInput`。
- 用 I2S DAC、内置 codec、PWM class-D 功放或耳机 codec 实现 `AudioOutput`。
- 用 IMU 芯片驱动实现 `IMUInput`，例如 I2C/SPI 读取加速度计和陀螺仪。
- 用 GPIO debounce / interrupt / scan matrix 实现 `ButtonInput`。
- 保持 `MotionDetector`、`GestureToEffectMapper` 和 `EffectProcessor` 不直接依赖硬件驱动。
