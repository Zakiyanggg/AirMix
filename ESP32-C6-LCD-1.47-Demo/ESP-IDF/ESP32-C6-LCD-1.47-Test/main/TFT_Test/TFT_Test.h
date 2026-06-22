#pragma once

#include "esp_err.h"

// 1.8 inch 128x160 SPI TFT wiring for a classic ESP32/T32 dev board.
#define TFT_PIN_SCLK 18  // TFT SCL
#define TFT_PIN_MOSI 23  // TFT SDA
#define TFT_PIN_CS   5   // TFT CS
#define TFT_PIN_DC   16  // TFT DC
#define TFT_PIN_RST  17  // TFT RST
#define TFT_PIN_BLK  4   // TFT BLK

// TFT button wiring. Buttons are active-low and use internal pull-ups.
#define TFT_KEY1_PIN 32  // TFT K1
#define TFT_KEY2_PIN 33  // TFT K2
#define TFT_KEY3_PIN 25  // TFT K3
#define TFT_KEY4_PIN 26  // TFT K4

#define TFT_WIDTH    160
#define TFT_HEIGHT   128

esp_err_t TFT_Test_Init(void);
void TFT_Test_Run(void);
