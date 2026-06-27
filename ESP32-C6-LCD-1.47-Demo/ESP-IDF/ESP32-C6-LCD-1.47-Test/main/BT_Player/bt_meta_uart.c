#include "bt_meta_uart.h"

#if BT_PLAYER_OUTPUT_P4

#include <stdio.h>
#include <string.h>
#include "BT_Player.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "BT_META_UART";

esp_err_t bt_meta_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = BT_META_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(BT_META_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(BT_META_UART_NUM, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(BT_META_UART_NUM, BT_META_UART_TX_GPIO, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Meta UART TX GPIO%d @ %d baud -> P4 RX", BT_META_UART_TX_GPIO, BT_META_UART_BAUD);
    return ESP_OK;
}

static void sanitize_line(char *dst, size_t dst_len, const char *src)
{
    if (dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_len; i++) {
        char c = src[i];
        if (c == '\r' || c == '\n') {
            c = ' ';
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

void bt_meta_uart_publish(bool connected, bool playing, uint32_t sample_rate_hz,
                          const char *title, const char *artist)
{
    char safe_title[BT_PLAYER_TITLE_LEN];
    char safe_artist[BT_PLAYER_ARTIST_LEN];
    char line[160];

    sanitize_line(safe_title, sizeof(safe_title), title);
    sanitize_line(safe_artist, sizeof(safe_artist), artist);

    int n = snprintf(line, sizeof(line), "S %d %d %lu\n",
                     connected ? 1 : 0, playing ? 1 : 0, (unsigned long)sample_rate_hz);
    if (n > 0) {
        uart_write_bytes(BT_META_UART_NUM, line, n);
    }
    n = snprintf(line, sizeof(line), "T:%s\n", safe_title);
    if (n > 0) {
        uart_write_bytes(BT_META_UART_NUM, line, n);
    }
    n = snprintf(line, sizeof(line), "A:%s\n", safe_artist);
    if (n > 0) {
        uart_write_bytes(BT_META_UART_NUM, line, n);
    }
}

#endif
