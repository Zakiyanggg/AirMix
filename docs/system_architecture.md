# AirMix 系统架构

## 系统音频链路

```text
Phone / Music Source
        |
        | Bluetooth audio, future A2DP sink
        v
AudioInput interface
        |
        | audio buffer, float or fixed-point samples
        v
AudioEngine
        |
        v
EffectProcessor
        |
        | High-pass / Low-pass / Gate / Delay / Stutter / Ducking
        v
AudioOutput interface
        |
        +--> Internal speaker path: codec / DAC / amp / speaker
        |
        +--> 3.5mm headphone path: codec / DAC / headphone driver
```

第一阶段中，`AudioInput` 由模拟正弦波输入实现，`AudioOutput` 只记录和打印 buffer 的 RMS/peak。未来接入真实硬件时，只替换接口实现，不改变 motion、mapping 和 DSP 核心逻辑。

## 控制链路

```text
IMU sensor
   |
   v
IMUInput interface
   |
   v
MotionDetector
   |
   | GestureType
   v
GestureToEffectMapper ---- ButtonInput interface
   |                         |
   |                         v
   |                    ButtonFrame
   |
   +---- TapTempo
   |
   v
EffectParameters
   |
   v
EffectProcessor
```

控制链路与音频链路解耦。动作识别只输出语义手势，按钮只输出状态和边沿事件，映射层负责把它们变成音效参数。

## 模块职责

- `audio/AudioInput.h`：音频输入接口。未来由蓝牙 A2DP sink、USB audio、line-in 或文件流实现。
- `audio/AudioOutput.h`：音频输出接口。未来由 I2S codec、DAC、功放或耳机驱动实现。
- `audio/AudioEngine.h`：把输入、效果处理和输出串成每个 audio block 的处理流程。
- `audio/EffectProcessor.h`：根据 `EffectParameters` 对 buffer 应用 DSP 效果。
- `audio/filters/LowPassFilter.h`：MVP 一阶 low-pass filter。
- `audio/filters/HighPassFilter.h`：MVP 一阶 high-pass filter。
- `audio/effects/GateEffect.h`：跟随 tap tempo 的节奏静音效果。
- `audio/effects/DelayEffect.h`：基础 echo / delay throw。
- `audio/effects/StutterEffect.h`：短 buffer 重复播放。
- `motion/IMUInput.h`：IMU 输入接口和模拟实现。
- `motion/MotionDetector.h`：阈值式手势检测。
- `motion/GestureType.h`：手势枚举和字符串转换。
- `controls/ButtonInput.h`：按钮输入接口和模拟实现。
- `controls/ControlState.h`：效果模式和控制状态。
- `mapping/GestureToEffectMapper.h`：动作、按钮、tempo 到 DSP 参数的映射。
- `tempo/TapTempo.h`：tap tempo 估计。

## 数据流

1. 主循环或音频回调读取一个 `AudioBuffer`。
2. 控制任务读取一帧 `IMUSample` 和 `ButtonFrame`。
3. `MotionDetector` 根据 IMU 阈值输出 `GestureType`。
4. `TapTempo` 在用户按下 Tap 时更新 BPM。
5. `GestureToEffectMapper` 结合手势、按钮和 tempo，输出 `EffectParameters`。
6. `EffectProcessor` 对当前 audio buffer 做滤波、gate、delay、stutter 或 ducking。
7. `AudioOutput` 把处理后的 buffer 送到扬声器或耳机路径。

## 未来接入真实蓝牙

- 在支持蓝牙音频的平台上实现 `AudioInput`。
- ESP32 可优先评估 A2DP sink + I2S 输出链路。
- Linux / Raspberry Pi 可先用 BlueZ / PulseAudio / PipeWire 做桌面验证。
- 输入实现需要负责 sample rate、channel count、buffer underrun、reconnect 和音频格式转换。

## 未来接入真实 IMU

- 用 I2C 或 SPI 驱动 IMU 芯片，实现 `IMUInput::read()`。
- 驱动层只返回标准化单位：
  - 加速度：g。
  - 陀螺仪：degree per second。
  - `dt`：秒。
- `MotionDetector` 不直接读取寄存器，避免和具体传感器绑定。

## 未来接入 DAC、功放和 3.5mm 输出

- 用 I2S codec 或 DAC 实现 `AudioOutput`。
- 内置扬声器路径需要 class-D 功放、音量保护和可能的 limiter。
- 3.5mm 路径需要耳机驱动、插入检测、输出阻抗和 pop noise 控制。
- 如果扬声器和耳机可同时存在，`AudioOutput` 可拆成 mixer/router，让输出策略独立于 DSP。

