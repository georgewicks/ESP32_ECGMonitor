/**
 * @file ADCCapture.h
 * @author  george.r.wicks@gmail.com
 * @brief   ADC capture functionality - Additions from Google AI 
 * suggestions
 * @version 0.1
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma  once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "ads111x.h"
#include "ds3231.h"
#include "driver/gpio.h"
#include "spoof_i2cdev.h"

#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000

// TODO: move to ads111x.h
#define ADS1115_I2C_ADDR            0x48
#define ADS1115_CONV_REG            0x00
#define ADS1115_CFG_REG             0x01

// TODO: move to ds3231.h
#define DS3231_I2C_ADDR             0x68
#define DS3231_TIME_REG             0x00

#define ESP_INTR_FLAG_DEFAULT       0

/**
 * @brief data structures for ADC capture and timestamping for SD recording
 * 
 */
typedef struct {
    uint64_t micro_timestamp; // Microsecond accuracy for ECG spacing
    int16_t channels[4];
} adc_sample_t;

/**
 * @brief  ADC samples for ECG capture
 * 
 */
#define     NUM_CHANNELS        (4)
#define     NUM_SAMPLES         (215)       // 4 * 215 = 860 SPS

typedef struct {
    uint64_t    micro_timestamp;
    uint16_t    samples[NUM_CHANNELS][NUM_SAMPLES];
} ECG_adc_sample_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
} rtc_time_t;

