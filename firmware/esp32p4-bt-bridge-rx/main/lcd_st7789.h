#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

esp_err_t lcd_st7789_init(void);
esp_lcd_panel_handle_t lcd_st7789_get_panel(void);
esp_lcd_panel_io_handle_t lcd_st7789_get_io(void);
