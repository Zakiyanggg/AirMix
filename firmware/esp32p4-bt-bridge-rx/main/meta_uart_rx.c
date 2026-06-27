#include "meta_uart_rx.h"

#include <stdio.h>
#include <string.h>

#include "bt_bridge_pins.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_status.h"

static const char *TAG = "META_UART";

static void meta_uart_apply_state(int connected, int playing, unsigned long rate,
                                  const char *title, const char *artist)
{
    ui_status_set_bt_info(connected != 0, playing != 0, rate, title, artist);
}

static void meta_uart_parse_line(char *line)
{
    if (line[0] == '\0') {
        return;
    }

    static int s_connected;
    static int s_playing;
    static unsigned long s_rate;
    static char s_title[48];
    static char s_artist[48];

    if (line[0] == 'S' && line[1] == ' ') {
        unsigned long rate = 0;
        if (sscanf(line + 2, "%d %d %lu", &s_connected, &s_playing, &rate) >= 2) {
            s_rate = rate;
            meta_uart_apply_state(s_connected, s_playing, s_rate, s_title, s_artist);
        }
        return;
    }

    if (line[0] == 'T' && line[1] == ':') {
        strncpy(s_title, line + 2, sizeof(s_title) - 1);
        s_title[sizeof(s_title) - 1] = '\0';
        meta_uart_apply_state(s_connected, s_playing, s_rate, s_title, s_artist);
        return;
    }

    if (line[0] == 'A' && line[1] == ':') {
        strncpy(s_artist, line + 2, sizeof(s_artist) - 1);
        s_artist[sizeof(s_artist) - 1] = '\0';
        meta_uart_apply_state(s_connected, s_playing, s_rate, s_title, s_artist);
    }
}

static void meta_uart_rx_task(void *arg)
{
    (void)arg;
    char line[128];
    size_t line_len = 0;

    while (true) {
        uint8_t ch;
        int n = uart_read_bytes(BT_META_UART_NUM, &ch, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }

        if (ch == '\n') {
            line[line_len] = '\0';
            meta_uart_parse_line(line);
            line_len = 0;
            continue;
        }

        if (ch == '\r') {
            continue;
        }

        if (line_len + 1 < sizeof(line)) {
            line[line_len++] = (char)ch;
        }
    }
}

esp_err_t meta_uart_rx_init(void)
{
    uart_config_t cfg = {
        .baud_rate = BT_META_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(BT_META_UART_NUM, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(BT_META_UART_NUM, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(BT_META_UART_NUM, UART_PIN_NO_CHANGE, BT_META_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t ok = xTaskCreate(meta_uart_rx_task, "meta_uart_rx", 4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Meta UART RX GPIO%d @ %d baud <- ESP32 TX", BT_META_UART_RX_GPIO, BT_META_UART_BAUD);
    return ESP_OK;
}
