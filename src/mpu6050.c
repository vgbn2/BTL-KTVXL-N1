#include "mpu6050.h"
#include <string.h>

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
#include "stm32f1xx_hal.h"
extern I2C_HandleTypeDef hi2c1;

static bool platform_i2c_write(uint8_t addr, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr << 1), buf, 2, 50) == HAL_OK);
}

static bool platform_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
    if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr << 1), &reg, 1, 50) != HAL_OK) {
        return false;
    }
    return (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(addr << 1), buf, len, 50) == HAL_OK);
}

static void platform_delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}
#else
/* Host / Simulator Weak Stubs */
__attribute__((weak)) bool platform_i2c_write(uint8_t addr, uint8_t reg, uint8_t data) {
    (void)addr; (void)reg; (void)data;
    return true;
}

__attribute__((weak)) bool platform_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
    (void)addr;
    if (reg == MPU6050_REG_WHO_AM_I && len >= 1) {
        buf[0] = MPU6050_WHO_AM_I_VAL;
        return true;
    }
    if (reg == MPU6050_REG_ACCEL_XOUT_H && len >= 14) {
        memset(buf, 0, len);
        /* Default 1g on Z axis: 16384 = 0x4000 */
        buf[4] = 0x40; buf[5] = 0x00;
        return true;
    }
    return true;
}

__attribute__((weak)) void platform_delay_ms(uint32_t ms) {
    (void)ms;
}
#endif

bool mpu6050_init(mpu6050_dev_t *dev, uint8_t i2c_addr) {
    if (!dev) return false;
    dev->i2c_addr = i2c_addr ? i2c_addr : MPU6050_I2C_ADDR_DEFAULT;
    dev->is_initialized = false;

    /* Verify WHO_AM_I */
    uint8_t who_am_i = 0;
    if (!platform_i2c_read(dev->i2c_addr, MPU6050_REG_WHO_AM_I, &who_am_i, 1)) {
        return false;
    }
    if (who_am_i != MPU6050_WHO_AM_I_VAL && who_am_i != 0x72 && who_am_i != 0x70) {
        return false;
    }

    /* 1. Wake up from sleep, clock source = PLL with X Gyro (0x01) */
    if (!platform_i2c_write(dev->i2c_addr, MPU6050_REG_PWR_MGMT_1, 0x01)) {
        return false;
    }
    platform_delay_ms(10);

    /* 2. Sample rate divider: SMPLRT_DIV = 9 -> 1kHz / (1+9) = 100 Hz */
    if (!platform_i2c_write(dev->i2c_addr, MPU6050_REG_SMPLRT_DIV, 0x09)) {
        return false;
    }

    /* 3. DLPF config: DLPF_CFG = 3 (Accel 44Hz, Gyro 42Hz bandwidth) */
    if (!platform_i2c_write(dev->i2c_addr, MPU6050_REG_CONFIG, 0x03)) {
        return false;
    }

    /* 4. Gyro Config: FS_SEL = 0 (+/- 250 deg/s) */
    if (!platform_i2c_write(dev->i2c_addr, MPU6050_REG_GYRO_CONFIG, 0x00)) {
        return false;
    }

    /* 5. Accel Config: AFS_SEL = 0 (+/- 2g) */
    if (!platform_i2c_write(dev->i2c_addr, MPU6050_REG_ACCEL_CONFIG, 0x00)) {
        return false;
    }

    dev->is_initialized = true;
    return true;
}

bool mpu6050_read_all(mpu6050_dev_t *dev,
                      int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *temp,
                      int16_t *gx, int16_t *gy, int16_t *gz) {
    if (!dev || !dev->is_initialized) return false;

    uint8_t raw[14];
    if (!platform_i2c_read(dev->i2c_addr, MPU6050_REG_ACCEL_XOUT_H, raw, 14)) {
        return false;
    }

    if (ax) *ax = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    if (ay) *ay = (int16_t)(((uint16_t)raw[2] << 8) | raw[3]);
    if (az) *az = (int16_t)(((uint16_t)raw[4] << 8) | raw[5]);
    if (temp) *temp = (int16_t)(((uint16_t)raw[6] << 8) | raw[7]);
    if (gx) *gx = (int16_t)(((uint16_t)raw[8] << 8) | raw[9]);
    if (gy) *gy = (int16_t)(((uint16_t)raw[10] << 8) | raw[11]);
    if (gz) *gz = (int16_t)(((uint16_t)raw[12] << 8) | raw[13]);

    return true;
}

bool mpu6050_calibrate_offsets(mpu6050_dev_t *dev,
                               int16_t *ax_off, int16_t *ay_off, int16_t *az_off,
                               int16_t *gx_off, int16_t *gy_off, int16_t *gz_off,
                               uint16_t num_samples) {
    if (!dev || !dev->is_initialized || num_samples == 0) return false;

    int32_t sum_ax = 0, sum_ay = 0, sum_az = 0;
    int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;

    for (uint16_t i = 0; i < num_samples; i++) {
        int16_t raw_ax, raw_ay, raw_az, raw_temp, raw_gx, raw_gy, raw_gz;
        if (!mpu6050_read_all(dev, &raw_ax, &raw_ay, &raw_az, &raw_temp, &raw_gx, &raw_gy, &raw_gz)) {
            return false;
        }
        sum_ax += raw_ax;
        sum_ay += raw_ay;
        sum_az += raw_az;
        sum_gx += raw_gx;
        sum_gy += raw_gy;
        sum_gz += raw_gz;
        platform_delay_ms(5);
    }

    if (ax_off) *ax_off = (int16_t)(sum_ax / num_samples);
    if (ay_off) *ay_off = (int16_t)(sum_ay / num_samples);
    /* In flat position, 1g (16384) acts on Z, so offset is relative to 1g */
    if (az_off) *az_off = (int16_t)((sum_az / num_samples) - 16384);
    if (gx_off) *gx_off = (int16_t)(sum_gx / num_samples);
    if (gy_off) *gy_off = (int16_t)(sum_gy / num_samples);
    if (gz_off) *gz_off = (int16_t)(sum_gz / num_samples);

    return true;
}
