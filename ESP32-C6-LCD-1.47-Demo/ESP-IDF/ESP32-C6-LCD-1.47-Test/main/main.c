#include "BT_Player/BT_Player.h"
#include "TFT_Test/TFT_Test.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void ui_task(void *arg)
{
    (void)arg;
    TFT_Test_Run();
}

void app_main(void)
{
    ESP_ERROR_CHECK(TFT_Test_Init());
    ESP_ERROR_CHECK(BT_Player_Init());
    xTaskCreatePinnedToCore(ui_task, "tft_ui", 6144, NULL, 5, NULL, 1);
}
