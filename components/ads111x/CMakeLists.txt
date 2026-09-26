
idf_component_register(SRCS "ads111x.c"
                       INCLUDE_DIRS "."
                       REQUIRES "driver" "esp_driver_i2c"
                       PRIV_REQUIRES "main")
