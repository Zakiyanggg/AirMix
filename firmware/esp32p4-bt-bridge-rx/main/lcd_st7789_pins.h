#pragma once

#include "driver/gpio.h"

/* ST7789V 240x320 SPI — wire to P4 40-pin header (see docs/esp32_p4_spi_lcd_wiring.md) */
#define LCD_SPI_HOST                SPI2_HOST

#define LCD_H_RES                   240
#define LCD_V_RES                   320
#define LCD_PIXEL_CLOCK_HZ          (40 * 1000 * 1000)
#define LCD_CMD_BITS                8
#define LCD_PARAM_BITS              8

#define LCD_GPIO_MOSI               GPIO_NUM_20   /* DIN — right header GPIO20 */
#define LCD_GPIO_SCLK               GPIO_NUM_21   /* CLK — right header GPIO21 */
#define LCD_GPIO_CS                 GPIO_NUM_5    /* CS  — left header GPIO5 */
#define LCD_GPIO_DC                 GPIO_NUM_26   /* DC  — right header GPIO26 (no GPIO6 on this board) */
#define LCD_GPIO_RST                GPIO_NUM_22   /* RST — right header GPIO22 */
#define LCD_GPIO_BL                 GPIO_NUM_23   /* BL  — right header GPIO23; or tie BL to 3V3 */

/* Tune if image is shifted / wrong orientation */
#define LCD_GAP_X                   0
#define LCD_GAP_Y                   0
#define LCD_MIRROR_X                true
#define LCD_MIRROR_Y                true
#define LCD_SWAP_XY                 false

#define LCD_BL_ON_LEVEL             1
#define LCD_LVGL_DRAW_LINES         20
#define LCD_LVGL_TICK_MS            2

/* Verified 2026-06 with ColorChecker — see docs/esp32_p4_spi_lcd_wiring.md §已验证颜色配置 */
#define LCD_RGB_ELEMENT_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define LCD_PANEL_INVERT_COLOR      true
