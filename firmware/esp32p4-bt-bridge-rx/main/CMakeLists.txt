set(SRCS "main.c" "lcd_st7789.c" "lcd_colorchecker.c" "camera_spi_preview.c" "app_video.c" "ui_status.c" "meta_uart_rx.c")

idf_component_register(
    SRCS ${SRCS}
    PRIV_REQUIRES esp_driver_i2s esp_driver_gpio esp_driver_spi esp_driver_uart esp_lcd esp_timer
                  bsp_extra lvgl esp_video esp_driver_ppa esp_mm esp_driver_i2c
)
