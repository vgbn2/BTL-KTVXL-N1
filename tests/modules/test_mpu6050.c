#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "mpu6050.h"

int test_mpu6050_module(void) {
    int failed = 0;

    printf("  [RUN] test_mpu6050_constants\n");
    if (MPU6050_I2C_ADDR_DEFAULT != 0x68) failed++;
    if (MPU6050_WHO_AM_I_VAL != 0x68) failed++;
    if (MPU6050_REG_SMPLRT_DIV != 0x19) failed++;
    if (MPU6050_REG_CONFIG != 0x1A) failed++;
    if (MPU6050_REG_GYRO_CONFIG != 0x1B) failed++;
    if (MPU6050_REG_ACCEL_CONFIG != 0x1C) failed++;
    if (MPU6050_REG_ACCEL_XOUT_H != 0x3B) failed++;
    if (MPU6050_REG_PWR_MGMT_1 != 0x6B) failed++;
    if (MPU6050_REG_WHO_AM_I != 0x75) failed++;

    printf("  [RUN] test_mpu6050_init\n");
    mpu6050_dev_t dev;
    bool ok = mpu6050_init(&dev, MPU6050_I2C_ADDR_DEFAULT);
    if (!ok) failed++;
    if (!dev.is_initialized) failed++;
    if (dev.i2c_addr != 0x68) failed++;

    /* Null pointer guards */
    if (mpu6050_init(NULL, 0x68) != false) failed++;

    printf("  [RUN] test_mpu6050_read_all\n");
    int16_t ax = 0, ay = 0, az = 0;
    int16_t temp = 0;
    int16_t gx = 0, gy = 0, gz = 0;

    ok = mpu6050_read_all(&dev, &ax, &ay, &az, &temp, &gx, &gy, &gz);
    if (!ok) failed++;
    /* In host weak stub, default Z is 16384 (1g) and others are 0 */
    if (ax != 0) failed++;
    if (ay != 0) failed++;
    if (az != 16384) failed++;
    if (gx != 0) failed++;
    if (gy != 0) failed++;
    if (gz != 0) failed++;

    /* Injected non-zero raw sensor values test */
    dev.sim_ax = 123;
    dev.sim_ay = -456;
    dev.sim_az = 16000;
    dev.sim_gx = 10;
    dev.sim_gy = -20;
    dev.sim_gz = 30;
    ok = mpu6050_read_all(&dev, &ax, &ay, &az, &temp, &gx, &gy, &gz);
    if (!ok) failed++;
    if (ax != 123 || ay != -456 || az != 16000 || gx != 10 || gy != -20 || gz != 30) failed++;

    /* Pointer safety check: dev=NULL rejected, optional NULL axis allowed */
    if (mpu6050_read_all(NULL, &ax, &ay, &az, &temp, &gx, &gy, &gz) != false) failed++;
    if (mpu6050_read_all(&dev, NULL, &ay, &az, &temp, &gx, &gy, &gz) != true) failed++;

    printf("  [RUN] test_mpu6050_calibration_routine\n");
    /* 1. Flat stationary 1g sensor (ideal zero bias) */
    dev.sim_ax = 0;
    dev.sim_ay = 0;
    dev.sim_az = 16384;
    dev.sim_gx = 0;
    dev.sim_gy = 0;
    dev.sim_gz = 0;
    int16_t ax_off, ay_off, az_off;
    int16_t gx_off, gy_off, gz_off;

    ok = mpu6050_calibrate_offsets(&dev, &ax_off, &ay_off, &az_off,
                                  &gx_off, &gy_off, &gz_off, 10);
    if (!ok) failed++;
    /* Offset relative to 1g (16384 on Z): az - 16384 = 0 */
    if (ax_off != 0) failed++;
    if (ay_off != 0) failed++;
    if (az_off != 0) failed++;
    if (gx_off != 0) failed++;
    if (gy_off != 0) failed++;
    if (gz_off != 0) failed++;

    /* 2. Injected non-zero physical biases (e.g. ax=+50, ay=-30, az=16484 -> +100 LSB error) */
    dev.sim_ax = 50;
    dev.sim_ay = -30;
    dev.sim_az = 16484;
    dev.sim_gx = 15;
    dev.sim_gy = -25;
    dev.sim_gz = 8;
    ok = mpu6050_calibrate_offsets(&dev, &ax_off, &ay_off, &az_off,
                                   &gx_off, &gy_off, &gz_off, 20);
    if (!ok) failed++;
    if (ax_off != 50) failed++;
    if (ay_off != -30) failed++;
    if (az_off != 100) failed++; /* 16484 - 16384 = 100 */
    if (gx_off != 15) failed++;
    if (gy_off != -25) failed++;
    if (gz_off != 8) failed++;

    /* 3. Mathematical proof: subtracting offsets zeroes out the errors */
    if ((dev.sim_ax - ax_off) != 0) failed++;
    if ((dev.sim_ay - ay_off) != 0) failed++;
    if ((dev.sim_az - az_off) != 16384) failed++; /* Restored to exact 1.0g */
    if ((dev.sim_gx - gx_off) != 0) failed++;
    if ((dev.sim_gy - gy_off) != 0) failed++;
    if ((dev.sim_gz - gz_off) != 0) failed++;

    /* 4. Multi-sample mock buffer averaging with noise */
    const int16_t noisy_samples[4][6] = {
        { 48, -32, 16480, 14, -26, 7 },
        { 52, -28, 16488, 16, -24, 9 },
        { 49, -31, 16482, 15, -25, 8 },
        { 51, -29, 16486, 15, -25, 8 }
    };
    dev.sim_buffer = noisy_samples;
    dev.sim_buffer_len = 4;
    dev.sim_buffer_idx = 0;
    ok = mpu6050_calibrate_offsets(&dev, &ax_off, &ay_off, &az_off,
                                   &gx_off, &gy_off, &gz_off, 4);
    if (!ok) failed++;
    if (ax_off != 50) failed++;
    if (ay_off != -30) failed++;
    if (az_off != 100) failed++;
    if (gx_off != 15) failed++;
    if (gy_off != -25) failed++;
    if (gz_off != 8) failed++;
    dev.sim_buffer = NULL;
    dev.sim_buffer_len = 0;

    /* 5. Error & boundary cases */
    if (mpu6050_calibrate_offsets(&dev, &ax_off, &ay_off, &az_off,
                                  &gx_off, &gy_off, &gz_off, 0) != false) failed++;
    if (mpu6050_calibrate_offsets(NULL, &ax_off, &ay_off, &az_off,
                                  &gx_off, &gy_off, &gz_off, 10) != false) failed++;
    mpu6050_dev_t uninit_dev = {0};
    if (mpu6050_calibrate_offsets(&uninit_dev, &ax_off, &ay_off, &az_off,
                                  &gx_off, &gy_off, &gz_off, 10) != false) failed++;

    if (failed == 0) {
        printf("  [PASS] test_mpu6050_module (0 failures)\n");
    } else {
        fprintf(stderr, "  [FAIL] test_mpu6050_module (%d failures)\n", failed);
    }
    return failed;
}
