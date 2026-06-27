#include "ui_status.h"

#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "esp_check.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_st7789.h"
#include "lcd_st7789_pins.h"
#include "lvgl.h"

static const char *TAG = "UI_STATUS";

static _lock_t s_lvgl_lock;
static lv_display_t *s_display;
static lv_obj_t *s_label_brand;
static lv_obj_t *s_label_track;
static lv_obj_t *s_label_artist;
static lv_obj_t *s_label_status;
static lv_obj_t *s_label_stats;

static bool ui_status_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void ui_status_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, px_map);
}

static void ui_status_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LCD_LVGL_TICK_MS);
}

static void ui_status_lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        _lock_acquire(&s_lvgl_lock);
        uint32_t delay_ms = lv_timer_handler();
        _lock_release(&s_lvgl_lock);
        delay_ms = MAX(delay_ms, 1000 / CONFIG_FREERTOS_HZ);
        delay_ms = MIN(delay_ms, 500);
        usleep(delay_ms * 1000);
    }
}

static void ui_status_build_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE8F0FF), 0);

    s_label_brand = lv_label_create(scr);
    lv_label_set_text(s_label_brand, "AirMix");
    lv_obj_set_style_text_font(s_label_brand, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_label_brand, lv_color_hex(0x8899AA), 0);
    lv_obj_align(s_label_brand, LV_ALIGN_TOP_MID, 0, 8);

    s_label_track = lv_label_create(scr);
    lv_label_set_text(s_label_track, "Waiting for ESP32...");
    lv_obj_set_style_text_font(s_label_track, &lv_font_montserrat_16, 0);
    lv_obj_set_width(s_label_track, LCD_H_RES - 20);
    lv_obj_set_style_text_align(s_label_track, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_track, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_track, LV_ALIGN_TOP_MID, 0, 36);

    s_label_artist = lv_label_create(scr);
    lv_label_set_text(s_label_artist, "Pair phone with ESP32-AUDIO");
    lv_obj_set_style_text_font(s_label_artist, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_artist, lv_color_hex(0xAABBCC), 0);
    lv_obj_set_width(s_label_artist, LCD_H_RES - 20);
    lv_obj_set_style_text_align(s_label_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_artist, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_artist, LV_ALIGN_TOP_MID, 0, 100);

    s_label_status = lv_label_create(scr);
    lv_label_set_text(s_label_status, "BT idle");
    lv_obj_set_style_text_font(s_label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_width(s_label_status, LCD_H_RES - 20);
    lv_obj_set_style_text_align(s_label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_label_status, LV_ALIGN_CENTER, 0, 10);

    s_label_stats = lv_label_create(scr);
    lv_label_set_text(s_label_stats, "");
    lv_obj_set_style_text_font(s_label_stats, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_stats, lv_color_hex(0x667788), 0);
    lv_obj_set_width(s_label_stats, LCD_H_RES - 20);
    lv_obj_set_style_text_align(s_label_stats, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_label_stats, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void ui_status_set_bt_info(bool connected, bool playing, uint32_t sample_rate_hz,
                           const char *title, const char *artist)
{
    if (s_label_track == NULL) {
        return;
    }

    const char *show_title = (title != NULL && title[0] != '\0') ? title : "Unknown track";
    const char *show_artist = (artist != NULL && artist[0] != '\0') ? artist : "Unknown artist";

    char status[64];
    if (!connected) {
        snprintf(status, sizeof(status), "Bluetooth disconnected");
    } else if (playing) {
        snprintf(status, sizeof(status), "Playing @ %lu Hz", (unsigned long)sample_rate_hz);
    } else {
        snprintf(status, sizeof(status), "Connected / paused");
    }

    _lock_acquire(&s_lvgl_lock);
    lv_label_set_text(s_label_track, show_title);
    lv_label_set_text(s_label_artist, show_artist);
    lv_label_set_text(s_label_status, status);
    if (!connected) {
        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0xFFD166), 0);
    } else if (playing) {
        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0x7CFC98), 0);
    } else {
        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0x88CCFF), 0);
    }
    _lock_release(&s_lvgl_lock);
}

esp_err_t ui_status_init(void)
{
    ESP_RETURN_ON_ERROR(lcd_st7789_init(), TAG, "lcd init failed");

    esp_lcd_panel_handle_t panel = lcd_st7789_get_panel();
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel missing");

    lv_init();
    s_display = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_ERR_NO_MEM, TAG, "display create failed");

    size_t draw_buf_sz = LCD_H_RES * LCD_LVGL_DRAW_LINES * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, draw_buf_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, draw_buf_sz, 0);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, TAG, "draw buffer alloc failed");

    lv_display_set_buffers(s_display, buf1, buf2, draw_buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(s_display, panel);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, ui_status_flush_cb);

    esp_lcd_panel_io_handle_t io = lcd_st7789_get_io();
    ESP_RETURN_ON_FALSE(io != NULL, ESP_ERR_INVALID_STATE, TAG, "panel io missing");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = ui_status_flush_ready,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io, &cbs, s_display), TAG, "io cb failed");

    const esp_timer_create_args_t tick_args = {
        .callback = ui_status_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "tick timer failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, LCD_LVGL_TICK_MS * 1000), TAG, "tick start failed");

    _lock_acquire(&s_lvgl_lock);
    ui_status_build_screen();
    _lock_release(&s_lvgl_lock);

    BaseType_t ok = xTaskCreate(ui_status_lvgl_task, "ui_lvgl", 6144, NULL, 4, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "lvgl task failed");

    ESP_LOGI(TAG, "LVGL status UI ready");
    return ESP_OK;
}

void ui_status_set_audio_stats(uint32_t rx_bytes, uint32_t rx_delta_bytes)
{
    if (s_label_stats == NULL) {
        return;
    }

    char text[64];
    snprintf(text,
             sizeof(text),
             "PCM +%lu KB/5s",
             (unsigned long)(rx_delta_bytes / 1024));

    _lock_acquire(&s_lvgl_lock);
    lv_label_set_text(s_label_stats, text);
    _lock_release(&s_lvgl_lock);

    (void)rx_bytes;
}
