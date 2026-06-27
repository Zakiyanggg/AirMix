#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "BT_Player.h"
#include "esp_err.h"

#define BT_META_UART_NUM        UART_NUM_1
#define BT_META_UART_TX_GPIO    17
#define BT_META_UART_BAUD       115200

#if BT_PLAYER_OUTPUT_P4

esp_err_t bt_meta_uart_init(void);
void bt_meta_uart_publish(bool connected, bool playing, uint32_t sample_rate_hz,
                          const char *title, const char *artist);

#else

static inline esp_err_t bt_meta_uart_init(void)
{
    return ESP_OK;
}

static inline void bt_meta_uart_publish(bool connected, bool playing, uint32_t sample_rate_hz,
                                        const char *title, const char *artist)
{
    (void)connected;
    (void)playing;
    (void)sample_rate_hz;
    (void)title;
    (void)artist;
}

#endif
