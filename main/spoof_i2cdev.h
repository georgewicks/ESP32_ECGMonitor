/**
 * @file spoof_i2cdev.h
 * @author george.r.wicks@gmail.com
 * @brief   This is needed so that I can have a replacement i2c_dev_t structure since I am avoiding the esp_idf_lib version
 * @version 0.1
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"

// stripped down version
typedef struct
{
    i2c_port_t                  port;                 //!< I2C port number (e.g., I2C_NUM_0)
    uint16_t                    addr;                   //!< Device I2C address (e.g., 0x48 for 7-bit)
    i2c_master_dev_handle_t     devhandle;
} i2c_dev_t;

