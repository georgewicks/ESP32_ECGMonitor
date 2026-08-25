/**
 * @file common.h
 * @author George Wicks (george.r.wicks@gmail.com)
 * @brief  common foundation header file.
 * @version 0.1
 * @date 2025-06-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
// for the sdmmc support
#include <sys/stat.h>   
#include "esp_vfs_fat.h"        // must add REQUIRES "fatfs" to CMakeLists.txt
#include "sdmmc_cmd.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

#include <driver/gpio.h>

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif

#define GPIO_I2C_SDA        GPIO_NUM_4
#define GPIO_I2C_SCL        GPIO_NUM_5
#define GPIO_SDMMC_CLK      GPIO_NUM_39
#define GPIO_SDMMC_CMD      GPIO_NUM_40
#define GPIO_SDMMC_D0       GPIO_NUM_41
#define GPIO_SDMMC_D1       GPIO_NUM_42
#define GPIO_SDMMC_D2       GPIO_NUM_1
#define GPIO_SDMMC_D3       GPIO_NUM_2

