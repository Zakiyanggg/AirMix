#pragma once

#include "driver/gpio.h"

/* Must match ESP32 BT_Player.h and docs/esp32_p4_i2s_wiring.md */
#define BT_BRIDGE_I2S_RX_BCLK_GPIO  GPIO_NUM_2
#define BT_BRIDGE_I2S_RX_WS_GPIO    GPIO_NUM_3
#define BT_BRIDGE_I2S_RX_DIN_GPIO   GPIO_NUM_4

/* Typical A2DP rate from phone; change to 48000 if audio sounds wrong */
#define BT_BRIDGE_SAMPLE_RATE_HZ    44100
#define BT_BRIDGE_AUDIO_TASK_PRIO   23
#define BT_BRIDGE_READ_CHUNK_BYTES  2048

/* ESP32 meta UART TX (GPIO17) -> P4 RX */
#define BT_META_UART_NUM            UART_NUM_1
#define BT_META_UART_RX_GPIO        GPIO_NUM_27
#define BT_META_UART_BAUD           115200
