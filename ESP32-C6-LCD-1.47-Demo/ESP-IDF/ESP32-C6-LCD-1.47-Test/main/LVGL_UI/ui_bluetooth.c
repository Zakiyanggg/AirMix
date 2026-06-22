#include "ui_bluetooth.h"

#include <string.h>

#include "ui_common.h"

typedef struct {
    uint8_t y;
    uint8_t h;
} ui_wave_bar_t;

static const ui_wave_bar_t s_wave_bars[UI_BT_WAVE_COUNT] = {
    {9, 7}, {5, 11}, {2, 14}, {7, 9}, {3, 13}, {8, 8},
    {1, 15}, {6, 10}, {10, 6}, {4, 12}, {8, 8},
};

static const lv_coord_t s_wave_base_x = 6;
static const lv_coord_t s_wave_base_y = 52;

static void ui_split_title(const char *title, char *line1, size_t line1_sz, char *line2, size_t line2_sz)
{
    line1[0] = '\0';
    line2[0] = '\0';
    if (title == NULL || title[0] == '\0') {
        return;
    }

    size_t len = strlen(title);
    if (len < line1_sz) {
        strncpy(line1, title, line1_sz - 1);
        line1[line1_sz - 1] = '\0';
        return;
    }

    size_t split = len / 2;
    while (split > 0 && title[split] != ' ') {
        split--;
    }
    if (split == 0) {
        split = len / 2;
    }

    strncpy(line1, title, split);
    line1[split] = '\0';
    while (title[split] == ' ') {
        split++;
    }
    strncpy(line2, title + split, line2_sz - 1);
    line2[line2_sz - 1] = '\0';
}

static void ui_create_top_tabs(lv_obj_t *parent, UI_Bluetooth_t *ui)
{
    ui_box(parent, 2, 2, 76, 12, UI_C_TAB_BT, 0, 0);
    ui_box(parent, 82, 2, 76, 12, UI_C_TAB_TRK, 0, 0);

    ui->bt_dot = lv_obj_create(parent);
    lv_obj_remove_style_all(ui->bt_dot);
    lv_obj_set_size(ui->bt_dot, ui_px(4), ui_py(4));
    lv_obj_set_pos(ui->bt_dot, ui_px(8), ui_py(6));
    lv_obj_set_style_radius(ui->bt_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui->bt_dot, ui_color(UI_C_BLUE), 0);
    lv_obj_set_style_bg_opa(ui->bt_dot, LV_OPA_COVER, 0);

    lv_obj_t *tab_bt = ui_label(parent, 20, 3, "BLUETOOTH", UI_C_DARK, ui_font_xs(), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(tab_bt, ui_px(56));

    lv_obj_t *tab_track = ui_label(parent, 98, 3, "TRACK", UI_C_DARK, ui_font_xs(), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(tab_track, ui_px(44));
}

static void ui_create_left_panel(lv_obj_t *parent, UI_Bluetooth_t *ui)
{
    ui_box(parent, 4, 20, 46, 14, UI_C_BG, UI_C_MUTED, 1);
    ui_box(parent, 8, 23, 6, 8, UI_C_CREAM, 0, 0);
    ui_box(parent, 16, 23, 10, 8, UI_C_BG, UI_C_CREAM, 1);
    lv_obj_t *a2dp = ui_label(parent, 24, 22, "A2DP", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(a2dp, ui_px(26));

    for (int i = 0; i < UI_BT_WAVE_COUNT; i++) {
        lv_coord_t bar_x = s_wave_base_x + (lv_coord_t)(i * 4);
        lv_coord_t bar_y = s_wave_base_y + s_wave_bars[i].y;
        ui->wave_bars[i] = ui_box(parent, bar_x, bar_y, 3, s_wave_bars[i].h, UI_C_CREAM, 0, 0);
    }

    ui_box(parent, 4, 74, 46, 4, UI_C_BG, UI_C_BORDER, 1);
    ui->pos_fill = ui_box(parent, 4, 74, 17, 4, UI_C_CREAM, 0, 0);
    ui->pos_cur = ui_label(parent, 4, 82, "00:00", UI_C_CREAM, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui->pos_total = ui_label(parent, 28, 82, "00:00", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->pos_total, ui_px(22));

    ui_label(parent, 4, 93, "USER", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui->user_val = ui_label(parent, 28, 93, "01", UI_C_CREAM, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->user_val, ui_px(22));
}

static void ui_create_right_panel(lv_obj_t *parent, UI_Bluetooth_t *ui)
{
    ui->title_line1 = ui_label(parent, 84, 24, "ESP32-AUDIO", UI_C_CREAM, ui_font_sm(), LV_TEXT_ALIGN_LEFT);
    ui->title_line2 = ui_label(parent, 84, 35, "", UI_C_CREAM, ui_font_sm(), LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(ui->title_line1, ui_px(74));
    lv_obj_set_width(ui->title_line2, ui_px(74));
    lv_label_set_long_mode(ui->title_line1, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(ui->title_line2, LV_LABEL_LONG_DOT);

    ui_label(parent, 84, 49, "ARTIST", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui->artist_val = ui_label(parent, 84, 49, "PAIR & PLAY", UI_C_CREAM, ui_font_sm(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->artist_val, ui_px(74));

    ui_label(parent, 84, 61, "STYLE", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui->style_val = ui_label(parent, 84, 61, "NEON POP", UI_C_CREAM, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->style_val, ui_px(74));

    ui_label(parent, 84, 73, "FX", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui->fx_val = ui_label(parent, 84, 73, "LP -3", UI_C_TAB_TRK, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->fx_val, ui_px(74));

    ui_label(parent, 84, 87, "VOL", UI_C_MUTED, ui_font_xs(), LV_TEXT_ALIGN_LEFT);
    ui_box(parent, 102, 87, 38, 7, UI_C_BG, UI_C_BORDER, 1);
    ui->vol_fill = ui_box(parent, 102, 87, 23, 7, UI_C_CREAM, 0, 0);
    ui->vol_val = ui_label(parent, 84, 87, "60", UI_C_CREAM, ui_font_xs(), LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(ui->vol_val, ui_px(74));
}

void UI_Bluetooth_Create(lv_obj_t *parent, UI_Bluetooth_t *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui_fonts_init();

    lv_obj_set_style_bg_color(parent, ui_color(UI_C_BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_top_tabs(parent, ui);
    ui_create_left_panel(parent, ui);
    ui_create_right_panel(parent, ui);
    ui_bottom_bar(parent, UI_C_CREAM, UI_C_DARK, UI_C_TAB_TRK, UI_C_DARK, "62%");

    lv_obj_t *divider = lv_line_create(parent);
    static lv_point_t divider_pts[] = {{0, 0}, {0, 92}};
    lv_line_set_points(divider, divider_pts, 2);
    lv_obj_set_pos(divider, ui_px(80), ui_py(18));
    lv_obj_set_style_line_color(divider, ui_color(UI_C_DIVIDER), 0);
    lv_obj_set_style_line_width(divider, 1, 0);
}

static void ui_set_wave_bars(UI_Bluetooth_t *ui, bool animate)
{
    static uint8_t wave_phase;

    if (animate) {
        wave_phase++;
    }

    for (int i = 0; i < UI_BT_WAVE_COUNT; i++) {
        uint8_t h = s_wave_bars[i].h;
        uint8_t y = s_wave_bars[i].y;
        if (animate) {
            h = (uint8_t)(((h + wave_phase + (uint8_t)(i * 2)) % 8) + 5);
            y = (uint8_t)(16 - h);
        }
        lv_obj_set_height(ui->wave_bars[i], ui_py(h));
        lv_obj_set_y(ui->wave_bars[i], ui_py(s_wave_base_y + y));
    }
}

void UI_Bluetooth_Update(UI_Bluetooth_t *ui, const BT_Player_Status_t *status)
{
    if (ui == NULL || status == NULL) {
        return;
    }

    char line1[BT_PLAYER_TITLE_LEN];
    char line2[BT_PLAYER_TITLE_LEN];
    const char *title = "ESP32-AUDIO";

    if (status->metadata_valid && status->title[0] != '\0') {
        title = status->title;
    } else if (status->audio_started) {
        title = "PLAYING...";
    } else if (status->connected) {
        title = "CONNECTED";
    }

    ui_split_title(title, line1, sizeof(line1), line2, sizeof(line2));
    lv_label_set_text(ui->title_line1, line1);
    lv_label_set_text(ui->title_line2, line2);

    if (status->metadata_valid && status->artist[0] != '\0') {
        lv_label_set_text(ui->artist_val, status->artist);
    } else if (status->audio_started) {
        lv_label_set_text(ui->artist_val, "UNKNOWN");
    } else if (status->connected) {
        lv_label_set_text(ui->artist_val, "READY");
    } else {
        lv_label_set_text(ui->artist_val, "PAIR & PLAY");
    }

    lv_obj_set_style_bg_color(ui->bt_dot,
                              status->connected ? ui_color(UI_C_BLUE) : ui_color(UI_C_MUTED),
                              0);

    if (status->audio_started) {
        ui_set_wave_bars(ui, true);
        lv_label_set_text(ui->pos_cur, "PLAY");
        lv_obj_set_width(ui->pos_fill, ui_px(30));
    } else {
        ui_set_wave_bars(ui, false);
        lv_label_set_text(ui->pos_cur, "00:00");
        lv_obj_set_width(ui->pos_fill, ui_px(6));
    }
}
