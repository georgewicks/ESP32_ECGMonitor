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

/**
 * @brief The following are the I2C device addresses that we will use
 * 
 */
#define ADS111X_ADDR        0x48 //!< I2C device address with ADDR pin connected to ground
#define ADS111X_ADDR_B      0x49 //!< second ADS1115, ADDR pin connected to VDD
#define PCF8574_ADDR		0x27
#define DS3231_EEPROM_ADDR  0x57 //!< I2C address
#define DS3231_ADDR         0x68 //!< I2C address
#define MPU6050_ADDR        0x69 //!< MPU6050 (AD0 -> VDD; 0x68 is taken by the DS3231)

// I2C GPIO 
#define GPIO_I2C_SDA            GPIO_NUM_4
#define GPIO_I2C_SCL            GPIO_NUM_5
//#define GPIO_I2C_ALERT          GPIO_NUM_6
#define GPIO_I2C_ALERT          GPIO_NUM_21

// ADS1115 ALERT/RDY (conversion-ready) interrupt inputs. Open-drain, active-low,
// require a pull-up (enabled in Setup_ADC_SD).
#define ADS_ALERT_GPIO_A        GPIO_NUM_21   // ADS1115 #1 (0x48)
#define ADS_ALERT_GPIO_B        GPIO_NUM_7    // ADS1115 #2 (0x49); GPIO7 was formerly GPIO_I2C_PCF8574_INT

#define GPIO_I2C_PCF8574_INT    GPIO_NUM_7

// NEW defines for SPI as per rec from ESP Docs AI 
#define PIN_NUM_MISO 15
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   16


#define GPIO_SDMMC_CLK          GPIO_NUM_39
#define GPIO_SDMMC_CMD          GPIO_NUM_40
#define GPIO_SDMMC_D0           GPIO_NUM_41
#define GPIO_SDMMC_D1           GPIO_NUM_42
#define GPIO_SDMMC_D2           GPIO_NUM_1
#define GPIO_SDMMC_D3           GPIO_NUM_2

