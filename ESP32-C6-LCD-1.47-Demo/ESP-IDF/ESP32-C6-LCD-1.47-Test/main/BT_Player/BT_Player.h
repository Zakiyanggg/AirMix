#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* 1 = A2DP PCM -> I2S Master TX -> ESP32-P4 (no local MAX98357A)
 * 0 = legacy path with on-board MAX98357A + drum mix on ESP32 */
#define BT_PLAYER_OUTPUT_P4     1

#define BT_PLAYER_I2S_BCLK_GPIO 27
#define BT_PLAYER_I2S_LRC_GPIO  14
#define BT_PLAYER_I2S_DOUT_GPIO 22

#if !BT_PLAYER_OUTPUT_P4
#define BT_PLAYER_AMP_SD_GPIO   21
#endif

/* P4 side (I2S1 slave RX) — wire now, firmware in next step */
#define P4_I2S_RX_BCLK_GPIO     2
#define P4_I2S_RX_WS_GPIO       3
#define P4_I2S_RX_DIN_GPIO      4

#define BT_PLAYER_DEVICE_NAME   "ESP32-AUDIO"
#define BT_PLAYER_TITLE_LEN     48
#define BT_PLAYER_ARTIST_LEN    48

/* P4 bridge meta UART: ESP32 TX GPIO17 -> P4 GPIO27 RX @ 115200 */
#define BT_META_UART_TX_GPIO    17

typedef struct {
    bool bt_ready;
    bool connected;
    bool audio_started;
    bool amp_enabled;
    uint32_t packet_count;
    uint32_t i2s_write_count;
    uint32_t sample_rate_hz;
    uint32_t buffered_bytes;
    bool metadata_valid;
    char title[BT_PLAYER_TITLE_LEN];
    char artist[BT_PLAYER_ARTIST_LEN];
} BT_Player_Status_t;

typedef enum {
    BT_PLAYER_DRUM_SNARE = 0,
    BT_PLAYER_DRUM_KICK,
    BT_PLAYER_DRUM_OPEN_HAT,
    BT_PLAYER_DRUM_CLOSED_HAT,
} BT_Player_Drum_t;

esp_err_t BT_Player_Init(void);
void BT_Player_GetStatus(BT_Player_Status_t *status);
void BT_Player_TriggerDrum(BT_Player_Drum_t drum);
