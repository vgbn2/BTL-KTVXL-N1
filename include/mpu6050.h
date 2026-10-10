#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_I2C_ADDR_DEFAULT 0x68

/* Key registers */
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_INT_PIN_CFG  0x37
#define MPU6050_REG_INT_ENABLE   0x38
#define MPU6050_REG_INT_STATUS   0x3A
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75

#define MPU6050_WHO_AM_I_VAL     0x68

typedef struct {
    uint8_t i2c_addr;
    bool is_initialized;
} mpu6050_dev_t;

/**
 * @brief Initialize MPU6050 (wake up, 100Hz DLPF, +/-2g, +/-250 deg/s).
 */
bool mpu6050_init(mpu6050_dev_t *dev, uint8_t i2c_addr);

/**
 * @brief Burst read 14 bytes: Accel (X,Y,Z), Temp, Gyro (X,Y,Z).
 */
bool mpu6050_read_all(mpu6050_dev_t *dev,
                      int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *temp,
                      int16_t *gx, int16_t *gy, int16_t *gz);

/**
 * @brief Perform stationary calibration by averaging samples.
 */
bool mpu6050_calibrate_offsets(mpu6050_dev_t *dev,
                               int16_t *ax_off, int16_t *ay_off, int16_t *az_off,
                               int16_t *gx_off, int16_t *gy_off, int16_t *gz_off,
                               uint16_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
