#include "TFT_Test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "BT_Player.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui_bluetooth.h"

#define TFT_HOST SPI2_HOST
#define TFT_LVGL_TICK_MS 2
#define TFT_LVGL_BUF_LINES 20

#define TFT_KEY_ACTIVE_LEVEL 0
#define TFT_KEY_COUNT 4
#define TFT_KEY_DEBOUNCE_US 18000
#define TFT_KEY_QUEUE_LEN 16
#define TFT_KEY1_GPIO GPIO_NUM_32
#define TFT_KEY2_GPIO GPIO_NUM_33
#define TFT_KEY3_GPIO GPIO_NUM_25
#define TFT_KEY4_GPIO GPIO_NUM_26

#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_INVOFF  0x20
#define ST7735_NORON   0x13
#define ST7735_DISPON  0x29

static const char *TAG = "TFT_TEST";
static spi_device_handle_t tft_spi;

static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_color_t buf1[TFT_WIDTH * TFT_LVGL_BUF_LINES];
static lv_color_t buf2[TFT_WIDTH * TFT_LVGL_BUF_LINES];
static UI_Bluetooth_t s_bt_ui;
static lv_timer_t *ui_timer;
static uint32_t frame_count;
static QueueHandle_t key_event_queue;
static int64_t key_last_fire_us[TFT_KEY_COUNT];

static const gpio_num_t key_gpios[TFT_KEY_COUNT] = {
    TFT_KEY1_GPIO,
    TFT_KEY2_GPIO,
    TFT_KEY3_GPIO,
    TFT_KEY4_GPIO,
};

static const BT_Player_Drum_t key_drum_ids[TFT_KEY_COUNT] = {
    BT_PLAYER_DRUM_SNARE,
    BT_PLAYER_DRUM_KICK,
    BT_PLAYER_DRUM_OPEN_HAT,
    BT_PLAYER_DRUM_CLOSED_HAT,
};

static esp_err_t tft_write_cmd(uint8_t cmd);
static esp_err_t tft_write_data(const void *data, size_t len);
static esp_err_t tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static void tft_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void tft_lvgl_tick_cb(void *arg);
static void key_gpio_init(void);
static void key_task(void *arg);
static void key_isr_handler(void *arg);
static void create_ui(void);
static void ui_timer_cb(lv_timer_t *timer);

esp_err_t TFT_Test_Init(void)
{
    ESP_LOGI(TAG, "Init 1.8 inch 160x128 landscape SPI TFT with LVGL");
    ESP_LOGI(TAG, "TFT pins: SCL/SCLK=%d SDA/MOSI=%d CS=%d DC=%d RST=%d BLK=%d",
             TFT_PIN_SCLK, TFT_PIN_MOSI, TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, TFT_PIN_BLK);
    ESP_LOGI(TAG, "Keys: K1=%d K2=%d K3=%d K4=%d, active low",
             TFT_KEY1_GPIO, TFT_KEY2_GPIO, TFT_KEY3_GPIO, TFT_KEY4_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TFT_PIN_DC) | (1ULL << TFT_PIN_RST) | (1ULL << TFT_PIN_BLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio config failed");

    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_BLK, 0), TAG, "backlight off failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_RST, 0), TAG, "rst low failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_RST, 1), TAG, "rst high failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = TFT_PIN_SCLK,
        .mosi_io_num = TFT_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_LVGL_BUF_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(TFT_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "spi bus init failed");

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = TFT_PIN_CS,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(TFT_HOST, &dev_cfg, &tft_spi), TAG, "spi add device failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_SWRESET), TAG, "sw reset failed");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_SLPOUT), TAG, "sleep out failed");
    vTaskDelay(pdMS_TO_TICKS(150));

    uint8_t colmod = 0x05; // RGB565
    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_COLMOD), TAG, "colmod cmd failed");
    ESP_RETURN_ON_ERROR(tft_write_data(&colmod, 1), TAG, "colmod data failed");

    uint8_t madctl = 0xA0; // Landscape 90°: MY + MV, RGB color order.
    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_MADCTL), TAG, "madctl cmd failed");
    ESP_RETURN_ON_ERROR(tft_write_data(&madctl, 1), TAG, "madctl data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_INVOFF), TAG, "invert off failed");
    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_NORON), TAG, "normal display failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_DISPON), TAG, "display on failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_BLK, 1), TAG, "backlight on failed");

    key_gpio_init();

    lv_init();
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, TFT_WIDTH * TFT_LVGL_BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = tft_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    lv_timer_set_period(disp->refr_timer, 200);

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &tft_lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_timer_args, &tick_timer), TAG, "create tick timer failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, TFT_LVGL_TICK_MS * 1000), TAG, "start tick timer failed");

    create_ui();
    return ESP_OK;
}

void TFT_Test_Run(void)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static esp_err_t tft_write_cmd(uint8_t cmd)
{
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_DC, 0), TAG, "dc cmd failed");
    return spi_device_polling_transmit(tft_spi, &trans);
}

static esp_err_t tft_write_data(const void *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }

    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(TFT_PIN_DC, 1), TAG, "dc data failed");
    return spi_device_polling_transmit(tft_spi, &trans);
}

static esp_err_t tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_CASET), TAG, "caset failed");
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    ESP_RETURN_ON_ERROR(tft_write_data(data, sizeof(data)), TAG, "caset data failed");

    ESP_RETURN_ON_ERROR(tft_write_cmd(ST7735_RASET), TAG, "raset failed");
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    ESP_RETURN_ON_ERROR(tft_write_data(data, sizeof(data)), TAG, "raset data failed");

    return tft_write_cmd(ST7735_RAMWR);
}

static void tft_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    LV_UNUSED(drv);

    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;
    static uint8_t line[TFT_WIDTH * 2];

    if (area->x1 < 0 || area->y1 < 0 || area->x2 >= TFT_WIDTH || area->y2 >= TFT_HEIGHT) {
        lv_disp_flush_ready(drv);
        return;
    }

    if (tft_set_window(area->x1, area->y1, area->x2, area->y2) != ESP_OK) {
        lv_disp_flush_ready(drv);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t color = lv_color_to16(color_map[y * width + x]);
            line[x * 2] = color >> 8;
            line[x * 2 + 1] = color & 0xFF;
        }
        if (tft_write_data(line, width * 2) != ESP_OK) {
            break;
        }
    }

    if (lv_disp_flush_is_last(drv)) {
        frame_count++;
    }
    lv_disp_flush_ready(drv);
}

static void tft_lvgl_tick_cb(void *arg)
{
    LV_UNUSED(arg);
    lv_tick_inc(TFT_LVGL_TICK_MS);
}

static void IRAM_ATTR key_isr_handler(void *arg)
{
    uint8_t idx = (uint8_t)(uintptr_t)arg;
    BaseType_t woken = pdFALSE;
    if (key_event_queue != NULL) {
        xQueueSendFromISR(key_event_queue, &idx, &woken);
    }
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void key_task(void *arg)
{
    LV_UNUSED(arg);
    uint8_t idx = 0;

    while (1) {
        if (xQueueReceive(key_event_queue, &idx, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (idx >= TFT_KEY_COUNT) {
            continue;
        }
        if (gpio_get_level(key_gpios[idx]) != TFT_KEY_ACTIVE_LEVEL) {
            continue;
        }

        int64_t now = esp_timer_get_time();
        if (now - key_last_fire_us[idx] < TFT_KEY_DEBOUNCE_US) {
            continue;
        }
        key_last_fire_us[idx] = now;
        BT_Player_TriggerDrum(key_drum_ids[idx]);
    }
}

static void key_gpio_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < TFT_KEY_COUNT; i++) {
        mask |= 1ULL << key_gpios[i];
        key_last_fire_us[i] = 0;
    }

    gpio_config_t key_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&key_conf));

    key_event_queue = xQueueCreate(TFT_KEY_QUEUE_LEN, sizeof(uint8_t));
    ESP_ERROR_CHECK(key_event_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    for (int i = 0; i < TFT_KEY_COUNT; i++) {
        ESP_ERROR_CHECK(gpio_set_intr_type(key_gpios[i], GPIO_INTR_NEGEDGE));
        ESP_ERROR_CHECK(gpio_isr_handler_add(key_gpios[i], key_isr_handler, (void *)(uintptr_t)i));
    }

    BaseType_t task_ok = xTaskCreate(key_task, "key_drum", 3072, NULL, 10, NULL);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

static void create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    UI_Bluetooth_Create(scr, &s_bt_ui);
    ui_timer = lv_timer_create(ui_timer_cb, 200, NULL);
    ui_timer_cb(ui_timer);
}

static void ui_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    BT_Player_Status_t bt_status = {0};
    BT_Player_GetStatus(&bt_status);
    UI_Bluetooth_Update(&s_bt_ui, &bt_status);
}
