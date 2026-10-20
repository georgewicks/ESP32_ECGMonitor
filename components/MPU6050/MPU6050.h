/**
 * @file MPU6050.h
 * @brief Minimal MPU6050 (GY-521) accelerometer + gyroscope driver.
 *
 * Uses the ESP-IDF v6.x i2c_master driver (driver/i2c_master.h), consistent
 * with I2C_Handling.c. The device handle is created on the shared I2C bus by
 * the caller and passed in here.
 *
 * NOTE: this project's DS3231 RTC owns address 0x68, so the MPU6050 must have
 * its AD0 pin tied to 3.3 V -> address 0x69.
 */

#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define MPU6050_ADDR_AD0_LOW    (0x68)  /* conflicts with the DS3231 here */
#define MPU6050_ADDR_AD0_HIGH   (0x69)  /* AD0 -> VDD; use this one        */

/* Full-scale selection used by init(): +/-4 g and +/-500 deg/s. */
#define MPU6050_ACCEL_LSB_PER_G      (8192.0f)   /* +/-4 g   */
#define MPU6050_GYRO_LSB_PER_DPS     (65.5f)     /* +/-500 deg/s */

/** One set of raw accelerometer + gyroscope readings. */
typedef struct {
    int16_t accel_x, accel_y, accel_z;   /* raw LSB */
    int16_t gyro_x,  gyro_y,  gyro_z;    /* raw LSB */
} mpu6050_raw_t;

/**
 * @brief Read the WHO_AM_I register (should be 0x68).
 * @param dev   initialized i2c_master device handle
 * @param who   output: the WHO_AM_I byte
 * @return ESP_OK on success
 */
esp_err_t mpu6050_who_am_i(i2c_master_dev_handle_t dev, uint8_t *who);

/**
 * @brief Wake the device and configure it for motion sensing.
 *
 * Configuration: sample rate 200 Hz (DLPF 21 Hz), accelerometer +/-4 g,
 * gyroscope +/-500 deg/s.
 *
 * @param dev   initialized i2c_master device handle
 * @return ESP_OK on success
 */
esp_err_t mpu6050_init(i2c_master_dev_handle_t dev);

/**
 * @brief Read accelerometer + gyroscope as one 14-byte burst.
 * @param dev   initialized i2c_master device handle
 * @param out   output readings
 * @return ESP_OK on success
 */
esp_err_t mpu6050_read_raw(i2c_master_dev_handle_t dev, mpu6050_raw_t *out);
