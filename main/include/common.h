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

// I2C GPIO 
#define GPIO_I2C_SDA            GPIO_NUM_4
#define GPIO_I2C_SCL            GPIO_NUM_5
#define GPIO_I2C_ALERT          GPIO_NUM_6

#define GPIO_I2C_PCF8574_INT    GPIO_NUM_7

// NEW defines for SPI as per rec from ESP Docs AI 
#define PIN_NUM_MISO 15
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   16

// OLD
// Define GPIO mappings for SPI
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10


#define GPIO_SDMMC_CLK          GPIO_NUM_39
#define GPIO_SDMMC_CMD          GPIO_NUM_40
#define GPIO_SDMMC_D0           GPIO_NUM_41
#define GPIO_SDMMC_D1           GPIO_NUM_42
#define GPIO_SDMMC_D2           GPIO_NUM_1
#define GPIO_SDMMC_D3           GPIO_NUM_2

