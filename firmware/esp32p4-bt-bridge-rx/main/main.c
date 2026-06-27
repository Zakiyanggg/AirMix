#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_rom_sys.h"

#if CONFIG_AIRMIX_LCD_COLORCHECKER
#include "lcd_colorchecker.h"
#elif CONFIG_AIRMIX_CAMERA_PREVIEW
#include "camera_spi_preview.h"
#else
#include "bt_bridge_pins.h"
#include "bsp_board_extra.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "meta_uart_rx.h"
#include "ui_status.h"
#endif

static const char *TAG = "BT_BRIDGE";

#if CONFIG_AIRMIX_BT_BRIDGE

static void bt_bridge_startup_task(void *arg);

static __attribute__((constructor)) void bt_bridge_boot_marker(void)
{
    esp_rom_printf("\n[AirMix] constructor\n");
}

static i2s_chan_handle_t s_i2s_rx_chan;
static volatile uint32_t s_rx_bytes;
static volatile uint32_t s_play_bytes;

static esp_err_t bt_bridge_codec_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "codec init failed");
    ESP_RETURN_ON_ERROR(
        bsp_extra_codec_set_fs(BT_BRIDGE_SAMPLE_RATE_HZ, 16, I2S_SLOT_MODE_STEREO),
        TAG,
        "codec set fs failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL), TAG, "codec volume failed");
    ESP_LOGI(TAG,
             "ES8311 ready: %d Hz stereo, volume=%d (max=%d)",
             BT_BRIDGE_SAMPLE_RATE_HZ,
             CODEC_DEFAULT_VOLUME,
             CODEC_VOLUME_MAX);
    return ESP_OK;
}

static esp_err_t bt_bridge_i2s_rx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 256;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &s_i2s_rx_chan), TAG, "i2s channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BT_BRIDGE_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BT_BRIDGE_I2S_RX_BCLK_GPIO,
            .ws = BT_BRIDGE_I2S_RX_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = BT_BRIDGE_I2S_RX_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_i2s_rx_chan, &std_cfg), TAG, "i2s std init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_i2s_rx_chan), TAG, "i2s enable failed");

    ESP_LOGI(TAG,
             "I2S0 slave RX: BCLK=GPIO%d WS=GPIO%d DIN=GPIO%d @ %d Hz",
             BT_BRIDGE_I2S_RX_BCLK_GPIO,
             BT_BRIDGE_I2S_RX_WS_GPIO,
             BT_BRIDGE_I2S_RX_DIN_GPIO,
             BT_BRIDGE_SAMPLE_RATE_HZ);
    return ESP_OK;
}

static void bt_bridge_audio_task(void *arg)
{
    (void)arg;
    uint8_t buf[BT_BRIDGE_READ_CHUNK_BYTES];

    while (true) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_i2s_rx_chan, buf, sizeof(buf), &bytes_read, portMAX_DELAY);
        if (ret != ESP_OK || bytes_read == 0) {
            continue;
        }

        s_rx_bytes += (uint32_t)bytes_read;

        size_t bytes_written = 0;
        ret = bsp_extra_i2s_write(buf, bytes_read, &bytes_written, 0);
        if (ret == ESP_OK) {
            s_play_bytes += (uint32_t)bytes_written;
        }
    }
}

static void bt_bridge_stats_task(void *arg)
{
    (void)arg;
    uint32_t last_rx = 0;
    uint32_t last_play = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        uint32_t rx = s_rx_bytes;
        uint32_t play = s_play_bytes;
        ESP_LOGI(TAG, "audio rx=%lu B (+ %lu) play=%lu B (+ %lu)",
                 (unsigned long)rx,
                 (unsigned long)(rx - last_rx),
                 (unsigned long)play,
                 (unsigned long)(play - last_play));
        if (rx == last_rx) {
            ESP_LOGW(TAG, "no I2S data from ESP32 — check wiring and that phone is playing via ESP32-AUDIO");
        }
        ui_status_set_audio_stats(rx, rx - last_rx);
        last_rx = rx;
        last_play = play;
    }
}

static void bt_bridge_startup_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    esp_rom_printf("[AirMix] chip rev v%d.%d\n", chip.revision / 100, chip.revision % 100);

    esp_err_t err;
    ESP_LOGI(TAG, "AirMix P4 BT bridge RX — play ESP32 A2DP on board speaker");

    err = ui_status_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(err));
    }

    err = meta_uart_rx_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "meta UART init failed: %s", esp_err_to_name(err));
    }

    err = bt_bridge_codec_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "codec init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    err = bt_bridge_i2s_rx_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    xTaskCreatePinnedToCore(bt_bridge_audio_task, "bt_bridge_audio", 6144, NULL, BT_BRIDGE_AUDIO_TASK_PRIO, NULL, 0);
    xTaskCreate(bt_bridge_stats_task, "bt_bridge_stats", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Waiting for PCM from ESP32. Pair phone with ESP32-AUDIO, then play music.");
    vTaskDelete(NULL);
}

#endif /* CONFIG_AIRMIX_BT_BRIDGE */

void app_main(void)
{
#if CONFIG_AIRMIX_LCD_COLORCHECKER
    esp_rom_printf("\n\n=== AirMix P4 LCD ColorChecker Test ===\n");
    esp_err_t err = lcd_colorchecker_show();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "color checker failed: %s", esp_err_to_name(err));
    }
#elif CONFIG_AIRMIX_CAMERA_PREVIEW
    esp_rom_printf("\n\n=== AirMix P4 OV5647 Camera Preview ===\n");
    esp_err_t err = camera_spi_preview_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera preview failed: %s", esp_err_to_name(err));
    }
#else
    esp_rom_printf("\n\n=== AirMix P4 BT bridge RX ===\n");
    esp_rom_printf("[AirMix] app_main\n");

    BaseType_t ok = xTaskCreate(bt_bridge_startup_task, "bt_bridge_start", 8192, NULL, 5, NULL);
    if (ok != pdPASS) {
        esp_rom_printf("[AirMix] FATAL: startup task create failed\n");
    }
#endif
}
