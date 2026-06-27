#include "camera_spi_preview.h"

#include <inttypes.h>
#include <string.h>

#include "app_video.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lcd_st7789.h"
#include "lcd_st7789_pins.h"

static const char *TAG = "CAM_PREVIEW";

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

static ppa_client_handle_t s_ppa;
static esp_lcd_panel_handle_t s_panel;
static void *s_lcd_rgb565[2];
static int s_lcd_buf_idx;
static size_t s_lcd_buf_bytes;
static size_t s_cache_line_size;
static SemaphoreHandle_t s_spi_done;
static int s_video_fd = -1;
static uint32_t s_frame_count;
static uint32_t s_drop_count;
static int64_t s_fps_last_us;
static i2c_master_bus_handle_t s_i2c_bus;

static bool camera_panel_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;

    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_spi_done, &wake);
    return wake == pdTRUE;
}

static esp_err_t camera_i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_7,
        .scl_io_num = GPIO_NUM_8,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

static void camera_frame_to_lcd(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t src_w,
                                uint32_t src_h, size_t camera_buf_len, void *user_data)
{
    (void)camera_buf_index;
    (void)camera_buf_len;
    (void)user_data;

    if (s_panel == NULL || s_lcd_rgb565[0] == NULL || s_ppa == NULL) {
        return;
    }

    const uint32_t dst_w = LCD_H_RES;
    const uint32_t dst_h = LCD_V_RES;
    const float scale_x = (float)dst_w / (float)src_w;
    const float scale_y = (float)dst_h / (float)src_h;

    if (xSemaphoreTake(s_spi_done, pdMS_TO_TICKS(100)) != pdTRUE) {
        s_drop_count++;
        return;
    }

    void *draw_buf = s_lcd_rgb565[s_lcd_buf_idx];

    ppa_srm_oper_config_t srm = {
        .in.buffer = camera_buf,
        .in.pic_w = src_w,
        .in.pic_h = src_h,
        .in.block_w = src_w,
        .in.block_h = src_h,
        .in.block_offset_x = 0,
        .in.block_offset_y = 0,
        .in.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

        .out.buffer = draw_buf,
        .out.buffer_size = s_lcd_buf_bytes,
        .out.pic_w = dst_w,
        .out.pic_h = dst_h,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = scale_x,
        .scale_y = scale_y,
        .mirror_x = false,
        .mirror_y = false,
        .rgb_swap = false,
        .byte_swap = false,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    if (ppa_do_scale_rotate_mirror(s_ppa, &srm) != ESP_OK) {
        ESP_LOGE(TAG, "PPA scale failed");
        xSemaphoreGive(s_spi_done);
        return;
    }

    esp_cache_msync(draw_buf, s_lcd_buf_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, dst_w, dst_h, draw_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_spi_done);
        return;
    }

    s_lcd_buf_idx ^= 1;

    s_frame_count++;
    int64_t now = esp_timer_get_time();
    if (now - s_fps_last_us >= 1000000) {
        ESP_LOGI(TAG, "preview fps ~%lu drop=%lu (%lux%lu -> %lux%lu)",
                 (unsigned long)s_frame_count,
                 (unsigned long)s_drop_count,
                 (unsigned long)src_w,
                 (unsigned long)src_h,
                 (unsigned long)dst_w,
                 (unsigned long)dst_h);
        s_frame_count = 0;
        s_drop_count = 0;
        s_fps_last_us = now;
    }
}

esp_err_t camera_spi_preview_start(void)
{
    ESP_LOGI(TAG, "OV5647 -> ST7789 SPI preview (link test)");

    ESP_RETURN_ON_ERROR(lcd_st7789_init(), TAG, "lcd init failed");
    s_panel = lcd_st7789_get_panel();
    ESP_RETURN_ON_FALSE(s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel missing");

    esp_lcd_panel_io_handle_t io = lcd_st7789_get_io();
    ESP_RETURN_ON_FALSE(io != NULL, ESP_ERR_INVALID_STATE, TAG, "panel io missing");

    s_spi_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_spi_done != NULL, ESP_ERR_NO_MEM, TAG, "spi done sem failed");
    xSemaphoreGive(s_spi_done);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = camera_panel_trans_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io, &cbs, NULL), TAG, "io cb failed");

    /* LCD color: RGB + LE + panel invert — see docs/esp32_p4_spi_lcd_wiring.md */

    ESP_RETURN_ON_ERROR(camera_i2c_init(), TAG, "i2c init failed");

    ppa_client_config_t ppa_cfg = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_RETURN_ON_ERROR(ppa_register_client(&ppa_cfg, &s_ppa), TAG, "ppa client failed");
    ESP_RETURN_ON_ERROR(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_cache_line_size), TAG, "cache align failed");

    s_lcd_buf_bytes = ALIGN_UP(LCD_H_RES * LCD_V_RES * 2, s_cache_line_size);
    for (int i = 0; i < 2; i++) {
        s_lcd_rgb565[i] = heap_caps_aligned_calloc(s_cache_line_size, 1, s_lcd_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(s_lcd_rgb565[i] != NULL, ESP_ERR_NO_MEM, TAG, "lcd buffer alloc failed");
    }

    ESP_RETURN_ON_ERROR(app_video_main(s_i2c_bus), TAG, "video init failed");

    s_video_fd = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, APP_VIDEO_FMT);
    ESP_RETURN_ON_FALSE(s_video_fd >= 0, ESP_FAIL, TAG, "open MIPI CSI failed");

    void *camera_buf[2];
    for (int i = 0; i < 2; i++) {
        camera_buf[i] = heap_caps_aligned_calloc(
            s_cache_line_size, 1, app_video_get_buf_size(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(camera_buf[i] != NULL, ESP_ERR_NO_MEM, TAG, "camera buffer alloc failed");
    }

    ESP_RETURN_ON_ERROR(app_video_set_bufs(s_video_fd, 2, (const void **)camera_buf), TAG, "set camera bufs failed");
    ESP_RETURN_ON_ERROR(app_video_register_frame_operation_cb(camera_frame_to_lcd), TAG, "register cb failed");
    ESP_RETURN_ON_ERROR(app_video_stream_task_start(s_video_fd, 0, NULL), TAG, "stream start failed");

    s_fps_last_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Camera preview running on %dx%d ST7789", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}
