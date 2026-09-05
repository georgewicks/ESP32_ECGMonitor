
idf_component_register(SRCS "ECG_Testing.c" "main.c" "I2C_Handling.c" "ECG_WiFi.c"
                       INCLUDE_DIRS "include" "."
                       REQUIRES "spi_flash" "esp_wifi" "nvs_flash" "esp_event" "esp_timer" "driver" "log" "fatfs" "ads111x" "ds3231" "PCF8574" "QRSDetect"
                       PRIV_REQUIRES "spi_flash" "sdmmc")
