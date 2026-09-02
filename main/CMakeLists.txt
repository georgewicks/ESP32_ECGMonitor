
idf_component_register(SRCS "main.c" "I2C_Handling.c" "ECG_WiFi.c"
                       INCLUDE_DIRS "include" "."
                       REQUIRES "spi_flash" "esp_wifi" "nvs_flash" "esp_event" "esp_timer" "driver" "log" "fatfs" "ads111x" "ds3231"
                       PRIV_REQUIRES "spi_flash" "sdmmc")
