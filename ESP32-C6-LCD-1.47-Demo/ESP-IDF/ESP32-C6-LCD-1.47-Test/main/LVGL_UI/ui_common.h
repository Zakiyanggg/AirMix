#pragma once

#include "TFT_Test.h"
#include "lvgl.h"

#define UI_DESIGN_W 160
#define UI_DESIGN_H 128

#define UI_SCREEN_W TFT_WIDTH
#define UI_SCREEN_H TFT_HEIGHT

#define UI_C_BG      0x000000
#define UI_C_TAB_BT  0x6B6B6B
#define UI_C_TAB_TRK 0xB07A2A
#define UI_C_CREAM   0xE8DCC4
#define UI_C_MUTED   0x7A7A7A
#define UI_C_BORDER  0x5A5A5A
#define UI_C_DARK    0x0A0A0A
#define UI_C_DIVIDER 0x333333
#define UI_C_BLUE    0x2C5A9E
#define UI_C_RED     0xC73E2E
#define UI_C_BAT_BG  0x5C5C5C
#define UI_C_PANEL   0x2A2A2A

void ui_fonts_init(void);
extern const lv_font_t ui_font_zh_12;
const lv_font_t *ui_font_xs_get(void);
const lv_font_t *ui_font_sm_get(void);
const lv_font_t *ui_font_md_get(void);
const lv_font_t *ui_font_cjk_get(void);

static inline lv_coord_t ui_px(lv_coord_t value)
{
    return (lv_coord_t)((int32_t)value * UI_SCREEN_W / UI_DESIGN_W);
}

static inline lv_coord_t ui_py(lv_coord_t value)
{
    return (lv_coord_t)((int32_t)value * UI_SCREEN_H / UI_DESIGN_H);
}

static inline lv_color_t ui_color(uint32_t hex)
{
    return lv_color_hex(hex);
}

static inline const lv_font_t *ui_font_xs(void)
{
    return ui_font_xs_get();
}

static inline const lv_font_t *ui_font_sm(void)
{
    return ui_font_sm_get();
}

static inline const lv_font_t *ui_font_md(void)
{
    return ui_font_md_get();
}

static inline const lv_font_t *ui_font_cjk(void)
{
    return ui_font_cjk_get();
}

static inline void ui_label_compact(lv_obj_t *label)
{
    lv_obj_set_style_text_letter_space(label, -1, 0);
    lv_obj_set_style_text_line_space(label, -2, 0);
}

static inline lv_obj_t *ui_box(lv_obj_t *parent,
                               lv_coord_t x,
                               lv_coord_t y,
                               lv_coord_t w,
                               lv_coord_t h,
                               uint32_t fill_hex,
                               uint32_t border_hex,
                               lv_coord_t border_w)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, ui_px(x), ui_py(y));
    lv_obj_set_size(obj, ui_px(w), ui_py(h));
    lv_obj_set_style_bg_color(obj, ui_color(fill_hex), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    if (border_w > 0) {
        lv_obj_set_style_border_width(obj, border_w, 0);
        lv_obj_set_style_border_color(obj, ui_color(border_hex), 0);
    } else {
        lv_obj_set_style_border_width(obj, 0, 0);
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static inline lv_obj_t *ui_label(lv_obj_t *parent,
                                 lv_coord_t x,
                                 lv_coord_t y,
                                 const char *text,
                                 uint32_t color_hex,
                                 const lv_font_t *font,
                                 lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, ui_px(x), ui_py(y));
    lv_obj_set_style_text_color(label, ui_color(color_hex), 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_align(label, align, 0);
    ui_label_compact(label);
    return label;
}

static inline void ui_bottom_bar(lv_obj_t *parent,
                                 uint32_t fx_color,
                                 uint32_t fx_text_color,
                                 uint32_t drum_color,
                                 uint32_t drum_text_color,
                                 const char *battery_pct)
{
    ui_box(parent, 2, 114, 37, 12, fx_color, 0, 0);
    ui_label(parent, 12, 116, "FX", fx_text_color, ui_font_xs(), LV_TEXT_ALIGN_CENTER);

    ui_box(parent, 41, 114, 37, 12, drum_color, 0, 0);
    ui_label(parent, 50, 116, "DRUM", drum_text_color, ui_font_xs(), LV_TEXT_ALIGN_CENTER);

    ui_box(parent, 80, 114, 39, 12, UI_C_RED, 0, 0);
    ui_label(parent, 86, 116, "MELODY", UI_C_CREAM, ui_font_xs(), LV_TEXT_ALIGN_CENTER);

    ui_box(parent, 121, 114, 37, 12, UI_C_BAT_BG, 0, 0);
    ui_box(parent, 124, 117, 11, 6, UI_C_BAT_BG, UI_C_DARK, 1);
    ui_box(parent, 125, 118, 7, 4, UI_C_DARK, 0, 0);
    ui_box(parent, 135, 119, 1, 2, UI_C_DARK, 0, 0);
    lv_obj_t *bat_label = ui_label(parent, 118, 116, battery_pct, UI_C_DARK, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(bat_label, ui_px(37));
}
