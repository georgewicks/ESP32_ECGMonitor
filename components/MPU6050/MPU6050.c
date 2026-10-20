/**
 * @file MPU6050.c
 * @brief Minimal MPU6050 driver implementation (see MPU6050.h).
 *
 * Register map per the MPU-6000/MPU-6050 Product Specification (InvenSense).
 * Accelerometer and gyro are read together from 0x3B (14 bytes: accel X/Y/Z,
 * temperature, gyro X/Y/Z), all big-endian int16.
 */

#include "MPU6050.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Register addresses */
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_WHO_AM_I        0x75

#define MPU6050_WHO_AM_I_VALUE      0x68

/* Config register values */
#define MPU6050_SMPLRT_DIV_200HZ    4      /* 1 kHz / (1+4) = 200 Hz           */
#define MPU6050_CONFIG_DLPF_21HZ    0x04   /* DLPF_CFG=4 -> ~21 Hz bandwidth   */
#define MPU6050_GYRO_500DPS         0x08   /* FS_SEL=1 -> +/-500 deg/s         */
#define MPU6050_ACCEL_4G            0x08   /* AFS_SEL=1 -> +/-4 g              */

static esp_err_t mpu_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, sizeof(buf), 100);
}

static esp_err_t mpu_read_regs(i2c_master_dev_handle_t dev, uint8_t reg,
                               uint8_t *dst, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, dst, len, 100);
}

esp_err_t mpu6050_who_am_i(i2c_master_dev_handle_t dev, uint8_t *who)
{
    if (dev == NULL || who == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return mpu_read_regs(dev, MPU6050_REG_WHO_AM_I, who, 1);
}

esp_err_t mpu6050_init(i2c_master_dev_handle_t dev)
{
    esp_err_t err;

    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Sanity check: confirm the device responds with its WHO_AM_I. */
    uint8_t who = 0;
    err = mpu6050_who_am_i(dev, &who);
    if (err != ESP_OK) {
        return err;
    }
    if (who != MPU6050_WHO_AM_I_VALUE) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Wake (CLKSEL=1: PLL with X-axis gyro reference). */
    err = mpu_write_reg(dev, MPU6050_REG_PWR_MGMT_1, 0x01);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    err = mpu_write_reg(dev, MPU6050_REG_SMPLRT_DIV, MPU6050_SMPLRT_DIV_200HZ);
    if (err != ESP_OK) {
        return err;
    }
    err = mpu_write_reg(dev, MPU6050_REG_CONFIG, MPU6050_CONFIG_DLPF_21HZ);
    if (err != ESP_OK) {
        return err;
    }
    err = mpu_write_reg(dev, MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_500DPS);
    if (err != ESP_OK) {
        return err;
    }
    return mpu_write_reg(dev, MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_4G);
}

esp_err_t mpu6050_read_raw(i2c_master_dev_handle_t dev, mpu6050_raw_t *out)
{
    if (dev == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[14];
    esp_err_t err = mpu_read_regs(dev, MPU6050_REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    out->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    out->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    out->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    /* buf[6..7] = temperature, skipped */
    out->gyro_x = (int16_t)((buf[8]  << 8) | buf[9]);
    out->gyro_y = (int16_t)((buf[10] << 8) | buf[11]);
    out->gyro_z = (int16_t)((buf[12] << 8) | buf[13]);
    return ESP_OK;
}
