#include "ui_common.h"

extern const lv_font_t ui_font_zh_12;

#if LV_FONT_MONTSERRAT_10
static lv_font_t s_font_sm;
#endif

#if LV_FONT_MONTSERRAT_8
static lv_font_t s_font_xs;
#endif

static bool s_ui_fonts_ready;

void ui_fonts_init(void)
{
    if (s_ui_fonts_ready) {
        return;
    }

#if LV_FONT_MONTSERRAT_10
    s_font_sm = lv_font_montserrat_10;
    s_font_sm.fallback = &ui_font_zh_12;
#endif

#if LV_FONT_MONTSERRAT_8
    s_font_xs = lv_font_montserrat_8;
    s_font_xs.fallback = &ui_font_zh_12;
#endif

    s_ui_fonts_ready = true;
}

const lv_font_t *ui_font_xs_get(void)
{
#if LV_FONT_MONTSERRAT_8
    return &s_font_xs;
#elif LV_FONT_MONTSERRAT_10
    return &lv_font_montserrat_10;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *ui_font_sm_get(void)
{
#if LV_FONT_MONTSERRAT_10
    return &s_font_sm;
#elif LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *ui_font_md_get(void)
{
#if LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#elif LV_FONT_MONTSERRAT_10
    return ui_font_sm_get();
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *ui_font_cjk_get(void)
{
    return &ui_font_zh_12;
}
