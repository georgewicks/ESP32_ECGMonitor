
# set(CMAKE_C_STANDARD 11)
idf_component_register(SRCS "ds3231.c"
                       INCLUDE_DIRS "."
                       REQUIRES "driver" "esp_driver_i2c"
                       PRIV_REQUIRES "main")
