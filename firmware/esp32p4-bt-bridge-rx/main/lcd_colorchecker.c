#include "lcd_colorchecker.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "lcd_st7789.h"
#include "lcd_st7789_pins.h"

static const char *TAG = "LCD_COLORCHECKER";

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb888_t;

/* sRGB reference values for ColorChecker Classic, layout matches physical chart (6 cols x 4 rows). */
static const rgb888_t s_patches[4][6] = {
    /* Row 1 — grayscale */
    {
        {243, 243, 242}, /* White */
        {200, 200, 200}, /* Neutral 8 */
        {160, 160, 160}, /* Neutral 6.5 */
        {122, 122, 121}, /* Neutral 5 */
        {85, 85, 85},    /* Neutral 3.5 */
        {52, 52, 52},    /* Black */
    },
    /* Row 2 — primaries / secondaries */
    {
        {56, 61, 150},   /* Blue */
        {70, 148, 73},   /* Green */
        {175, 54, 60},   /* Red */
        {231, 199, 31},  /* Yellow */
        {187, 86, 149},  /* Magenta */
        {0, 136, 170},   /* Cyan */
    },
    /* Row 3 — mixed hues */
    {
        {214, 126, 44},  /* Orange */
        {80, 91, 166},   /* Purplish Blue */
        {193, 90, 99},   /* Moderate Red */
        {94, 60, 108},   /* Purple */
        {157, 188, 64},  /* Yellow Green */
        {224, 163, 46},  /* Orange Yellow */
    },
    /* Row 4 — natural tones */
    {
        {115, 82, 68},   /* Dark Skin */
        {194, 150, 130}, /* Light Skin */
        {98, 122, 157},  /* Blue Sky */
        {87, 108, 67},   /* Foliage */
        {133, 128, 177}, /* Blue Flower */
        {103, 189, 170}, /* Bluish Green */
    },
};

static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void fill_rect_rgb565(uint16_t *fb, int fb_w, int x, int y, int w, int h, uint16_t color)
{
    for (int row = y; row < y + h; row++) {
        uint16_t *line = fb + row * fb_w + x;
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

/* 5-bit-wide glyphs, MSB = left pixel. */
static const uint8_t s_glyph_b[] = {
    0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E,
};
static const uint8_t s_glyph_w[] = {
    0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11,
};

static void draw_glyph_scaled(uint16_t *fb, int fb_w, int x, int y, int scale, const uint8_t *rows,
                              int row_count, int col_bits, uint16_t color)
{
    for (int r = 0; r < row_count; r++) {
        const uint8_t bits = rows[r];
        for (int c = 0; c < col_bits; c++) {
            if (bits & (1 << (col_bits - 1 - c))) {
                fill_rect_rgb565(fb, fb_w, x + c * scale, y + r * scale, scale, scale, color);
            }
        }
    }
}

static void draw_patch_label(uint16_t *fb, int fb_w, int patch_x, int patch_y, int patch_w, int patch_h,
                             const uint8_t *glyph, int glyph_rows, int glyph_cols, int scale, uint16_t color)
{
    const int glyph_w = glyph_cols * scale;
    const int glyph_h = glyph_rows * scale;
    const int lx = patch_x + (patch_w - glyph_w) / 2;
    const int ly = patch_y + (patch_h - glyph_h) / 2;
    draw_glyph_scaled(fb, fb_w, lx, ly, scale, glyph, glyph_rows, glyph_cols, color);
}

esp_err_t lcd_colorchecker_show(void)
{
    ESP_LOGI(TAG, "ColorChecker test on %dx%d ST7789 (no camera)", LCD_H_RES, LCD_V_RES);

    ESP_RETURN_ON_ERROR(lcd_st7789_init(), TAG, "lcd init failed");

    esp_lcd_panel_handle_t panel = lcd_st7789_get_panel();
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel missing");

    const int fb_w = LCD_H_RES;
    const int fb_h = LCD_V_RES;
    const size_t fb_bytes = (size_t)fb_w * fb_h * sizeof(uint16_t);

    uint16_t *fb = heap_caps_malloc(fb_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (fb == NULL) {
        fb = heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    ESP_RETURN_ON_FALSE(fb != NULL, ESP_ERR_NO_MEM, TAG, "framebuffer alloc failed");

    memset(fb, 0, fb_bytes);

    const int cols = 6;
    const int rows = 4;
    const int gap = 2;
    const int grid_w = fb_w - gap;
    const int grid_h = fb_h - gap;
    const int patch_w = (grid_w - (cols - 1) * gap) / cols;
    const int patch_h = (grid_h - (rows - 1) * gap) / rows;
    const int origin_x = (fb_w - (cols * patch_w + (cols - 1) * gap)) / 2;
    const int origin_y = (fb_h - (rows * patch_h + (rows - 1) * gap)) / 2;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const rgb888_t c = s_patches[row][col];
            const uint16_t color = rgb888_to_rgb565(c.r, c.g, c.b);
            const int x = origin_x + col * (patch_w + gap);
            const int y = origin_y + row * (patch_h + gap);
            fill_rect_rgb565(fb, fb_w, x, y, patch_w, patch_h, color);
        }
    }

    const int label_scale = 5;
    const int glyph_cols = 5;
    const int glyph_rows = 7;
    const uint16_t black = rgb888_to_rgb565(0, 0, 0);
    const uint16_t white = rgb888_to_rgb565(255, 255, 255);

    /* White patch (top-left): black "B" */
    draw_patch_label(fb, fb_w, origin_x, origin_y, patch_w, patch_h, s_glyph_b, glyph_rows, glyph_cols,
                     label_scale, black);
    /* Black patch (top-right): white "W" */
    draw_patch_label(fb, fb_w, origin_x + 5 * (patch_w + gap), origin_y, patch_w, patch_h, s_glyph_w,
                     glyph_rows, glyph_cols, label_scale, white);

    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, 0, fb_w, fb_h, fb), TAG, "draw failed");

    heap_caps_free(fb);
    ESP_LOGI(TAG, "ColorChecker drawn — white patch=B(black), black patch=W(white)");
    return ESP_OK;
}
