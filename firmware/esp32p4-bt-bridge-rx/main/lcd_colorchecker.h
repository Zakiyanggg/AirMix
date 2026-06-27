#pragma once

#include "esp_err.h"

/** Draw X-Rite ColorChecker Classic (6x4) on ST7789; no camera involved. */
esp_err_t lcd_colorchecker_show(void);
