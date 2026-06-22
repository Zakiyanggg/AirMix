# 硬件假设

## MVP 阶段假设

- 音频输入、音频输出、IMU 和按钮全部通过接口抽象。
- 第一版不绑定具体蓝牙芯片、MCU、codec、DAC、功放或 IMU 型号。
- DSP 使用 float 实现，后续再根据目标平台改为定点或 SIMD 优化。
- 默认采样率按 48 kHz 设计，block size 可在 64-256 samples 内调优。
- 第一版以单声道 buffer 验证链路，后续可扩展为 stereo interleaved 或 planar。

## 未来硬件模块

- 主控：ESP32-S3、STM32H7、Teensy 4.x、Raspberry Pi 或带蓝牙音频能力的 SoC。
- 蓝牙：支持 A2DP sink，或通过外部蓝牙音频模块输出 I2S/PCM。
- IMU：6-axis accelerometer + gyroscope，I2C 或 SPI。
- 音频 codec / DAC：支持 I2S 输入，最好带耳机驱动或 line out。
- 功放：class-D amplifier，带硬件 mute / shutdown 控制。
- 输出：内置扬声器、3.5mm TRS 耳机接口，可选插入检测。
- 电源：锂电池、充电管理、升压/降压、电源路径管理。

## 工程注意事项

- 蓝牙和音频时钟要避免 drift 导致 underrun/overrun。
- IMU 安装位置要尽量靠近握持中心，同时减少扬声器震动耦合。
- 耳机输出需要 pop/click 抑制和最大音量保护。
- 功放路径需要硬件 mute，避免启动和关机爆音。
- 外壳按键需要盲操作友好，Effect Mode、Hold、Tap、Bypass 手感应明显区分。

