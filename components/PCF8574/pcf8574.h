/**
 * @file pcf8574.h
 * @author your name (you@domain.com)
 * @brief PCF8574 API, designed by Google AI
 * @version 0.1
 * @date 2026-09-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t address;
} pcf8574_dev_t;

// Initialize the PCF8574 device on an existing master bus
esp_err_t pcf8574_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr, pcf8574_dev_t *dev);

// Write state to all 8 pins
esp_err_t pcf8574_write(pcf8574_dev_t *dev, uint8_t value);

// Read state from all 8 pins
esp_err_t pcf8574_read(pcf8574_dev_t *dev, uint8_t *value);
