# ESP32 ↔ ESP32-P4 蓝牙音乐直通

两板分别刷固件：ESP32 收 A2DP，P4 从 I2S 收 PCM，经 ES8311 板载喇叭播放。

## 接线

| ESP32 | → | ESP32-P4 |
|-------|---|----------|
| GPIO 27 (BCLK) | → | GPIO 2 |
| GPIO 14 (WS) | → | GPIO 3 |
| GPIO 22 (DOUT) | → | GPIO 4 |
| GND | → | GND |

喇叭接 **P4 板载 speaker**。MAX98357A **不接**。

## 刷机

### 1. ESP32（workspace ① ESP32）

```bash
cd projects/esp32-a2dp
idf.py set-target esp32
idf.py -B build-esp32 build flash monitor
```

串口应出现 `P4 bridge I2S Master TX` 和 `Ready. Pair phone with "ESP32-AUDIO"`。

### 2. ESP32-P4

```bash
cd firmware/esp32p4-bt-bridge-rx
idf.py set-target esp32p4
idf.py -B build-esp32p4 build flash monitor
```

串口应出现 `ES8311 ready` 和 `I2S1 slave RX`。

#### 构建报错排查

**A. `riscv32-esp-elf-gcc` not found**

P4 工具链未装入当前 IDF 环境，执行一次：

```bash
python $IDF_PATH/tools/idf_tools.py install --targets esp32p4
. $IDF_PATH/export.sh
```

**B. Bootloader binary size ... too large for partition table offset 0x8000**

已在 `sdkconfig.defaults` 里设 `CONFIG_PARTITION_TABLE_OFFSET=0x9000`。若仍报错：

```bash
rm -f sdkconfig
idf.py -B build-esp32p4 fullclean build
```

**C. `requires chip revision in range [v3.1 - v3.99] (this chip is revision v1.3)`**

你的 P4 是 **v1.x 工程样片**，不能用默认 v3.1 固件。必须用 pre-v3 overlay 重编：

```bash
cd firmware/esp32p4-bt-bridge-rx
rm -f sdkconfig
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.pre_v3" set-target esp32p4
idf.py -B build-esp32p4 build flash monitor
```

串口 `00_board_check` 或 esptool 连接时会显示 `Chip type: ESP32-P4 (revision v1.3)` → 用 pre_v3。  
若是 `revision v3.x` 则**不要**加 `sdkconfig.defaults.pre_v3`。

### 3. 使用

1. 共地 + I2S 三根线接好，P4 喇叭接好  
2. 两板上电  
3. 手机连 **ESP32-AUDIO**（PIN 1234）  
4. 播放音乐 → P4 喇叭出声  

P4 每 5 秒打印 `audio rx=...`；若不变，查接线或 ESP32 是否在播歌。

## 音调不对

改 P4 `main/bt_bridge_pins.h` 里 `BT_BRIDGE_SAMPLE_RATE_HZ` 为 **48000**，重刷 P4。对照 ESP32 串口 `I2S configured: sample_rate=...`。
