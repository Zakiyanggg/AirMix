#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t ui_status_init(void);
void ui_status_set_bt_info(bool connected, bool playing, uint32_t sample_rate_hz,
                           const char *title, const char *artist);
void ui_status_set_audio_stats(uint32_t rx_bytes, uint32_t rx_delta_bytes);
