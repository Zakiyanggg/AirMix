#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define BT_PLAYER_I2S_BCLK_GPIO 27
#define BT_PLAYER_I2S_LRC_GPIO  14
#define BT_PLAYER_I2S_DIN_GPIO  22
#define BT_PLAYER_AMP_SD_GPIO   21

#define BT_PLAYER_DEVICE_NAME   "ESP32-AUDIO"
#define BT_PLAYER_TITLE_LEN     48
#define BT_PLAYER_ARTIST_LEN    48

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
