#pragma once

#include "BT_Player.h"
#include "lvgl.h"

#define UI_BT_WAVE_COUNT 11

typedef struct {
    lv_obj_t *title_line1;
    lv_obj_t *title_line2;
    lv_obj_t *artist_val;
    lv_obj_t *style_val;
    lv_obj_t *fx_val;
    lv_obj_t *vol_val;
    lv_obj_t *user_val;
    lv_obj_t *pos_cur;
    lv_obj_t *pos_total;
    lv_obj_t *pos_fill;
    lv_obj_t *vol_fill;
    lv_obj_t *bt_dot;
    lv_obj_t *wave_bars[UI_BT_WAVE_COUNT];
} UI_Bluetooth_t;

void UI_Bluetooth_Create(lv_obj_t *parent, UI_Bluetooth_t *ui);
void UI_Bluetooth_Update(UI_Bluetooth_t *ui, const BT_Player_Status_t *status);
