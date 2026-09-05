/**
 * @file pcf8574.c
 * @author george.r.wicks@gmail.com
 * @brief  PCF8574 API, designed by Google AI
 * @version 0.1
 * @date 2026-09-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "pcf8574.h"

esp_err_t pcf8574_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, pcf8574_dev_t *dev) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    dev->address = i2c_addr;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        //.scl_speed_hz = 100000, // 100kHz standard mode  TODO - restore after test
        .scl_speed_hz = 10000, // 10kHz standard mode TEST!!
    };

    // Add device to the pre-initialized master bus
    return i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev->i2c_dev);
}

esp_err_t pcf8574_write(pcf8574_dev_t *dev, uint8_t value) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    // PCF8574 expects a single byte sent directly to its device address
    //return i2c_master_transmit(dev->i2c_dev, &value, 1, -1);
    return i2c_master_transmit(dev->i2c_dev, &value, 1, 500/portTICK_PERIOD_MS);
}

esp_err_t pcf8574_read(pcf8574_dev_t *dev, uint8_t *value) {
    if (!dev || !value) return ESP_ERR_INVALID_ARG;
    // PCF8574 returns a single byte containing pin statuses
    return i2c_master_receive(dev->i2c_dev, value, 1, -1);
}
