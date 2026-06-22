# MVP 路线图

## 阶段 1：软件模拟原型

- 建立文档、目录结构和接口边界。
- 完成模拟音频输入输出。
- 完成模拟 IMU 和按钮输入。
- 完成阈值式手势检测。
- 完成 tap tempo。
- 完成基础 DSP 效果和映射。
- 用控制台 demo 验证完整链路。

## 阶段 2：桌面音频验证

- 增加 WAV 文件输入和 WAV 文件输出。
- 增加实时音频 API 适配层，例如 PortAudio 或平台原生音频。
- 录制不同音乐片段，调试 cutoff、gate、delay、stutter 的手感。
- 加入简单 limiter，避免 demo 音频削波。

## 阶段 3：开发板 bring-up

- 选择目标开发板和 IMU 模块。
- 实现真实 `IMUInput`。
- 实现 I2S `AudioOutput`。
- 先用本地测试音频或 line-in 验证 audio callback 和 DMA。
- 再接入蓝牙音频输入。

## 阶段 4：交互调优

- 采集真实用户动作数据，调整阈值。
- 加入 gesture cooldown、debounce、简单滤波。
- 优化 Tap Tempo 的容错和视觉/触觉反馈。
- 调整 Hold、Bypass、Effect Mode 的状态反馈。

## 阶段 5：产品样机

- 打样外壳、按键、扬声器腔体和耳机口位置。
- 增加电池、电源、充电和保护电路。
- 完成基本可靠性测试：跌落、续航、热、蓝牙连接、耳机插拔。
- 评估量产 BOM、测试流程和固件升级方案。

