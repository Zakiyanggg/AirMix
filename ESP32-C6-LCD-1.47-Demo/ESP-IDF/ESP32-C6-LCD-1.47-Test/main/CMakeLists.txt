idf_component_register(
                         SRCS "main.c" 
                              "TFT_Test/TFT_Test.c"
                              "BT_Player/BT_Player.c"
                              "BT_Player/bt_meta_uart.c"
                              "LVGL_UI/ui_bluetooth.c"
                              "LVGL_UI/ui_common.c"
                              "LVGL_UI/ui_font_zh_12.c"

                         INCLUDE_DIRS 
                              "./TFT_Test"
                              "./BT_Player"
                              "./LVGL_UI"
                              "."
                         REQUIRES
                              bt
                              esp_driver_gpio
                              esp_driver_i2s
                              esp_driver_spi
                              esp_driver_uart
                              esp_ringbuf
                              esp_timer
                              lvgl__lvgl
                              nvs_flash
                       )
