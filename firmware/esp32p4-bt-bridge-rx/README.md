# ESP32-P4：接收 ESP32 I2S PCM 并播放

## 构建

你的 P4 是 **rev v1.3**，`sdkconfig.defaults` 已内置 pre-v3 配置。

**每次换配置或报 chip revision 错误，必须先删旧 sdkconfig 再 fullclean：**

```bash
cd firmware/esp32p4-bt-bridge-rx
rm -f sdkconfig
idf.py set-target esp32p4
idf.py -B build-esp32p4 fullclean build flash monitor
```

不要混用 `idf.py build`（写到 `build/`）和 `-B build-esp32p4`。

**不要用 `--force` 强刷 v3.1 固件**，会不匹配 v1.3 芯片。

## 串口

- P4 用 **USB 口** → `/dev/cu.usbmodem*`
- 正常启动应看到 `=== AirMix P4 BT bridge RX ===` 和 `ES8311 ready`
- 若只有 ROM 三行反复出现 → 固件崩溃或刷错版本，按上面命令 fullclean 重编

## 配置

采样率：`main/bt_bridge_pins.h` → `BT_BRIDGE_SAMPLE_RATE_HZ`（默认 44100）

GPIO：见 `docs/esp32_p4_bt_playback.md`
