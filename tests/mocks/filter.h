#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float alpha;            /* Complementary filter coefficient, e.g. 0.98f */
    float dt;               /* Sample period in seconds, e.g. 0.01f (100 Hz) */
    float pitch;            /* Filtered pitch angle in degrees */
    float roll;             /* Filtered roll angle in degrees */
    float yaw;              /* Integrated yaw angle in degrees */

    /* Calibration zero-offsets */
    int16_t accel_offset_x;
    int16_t accel_offset_y;
    int16_t accel_offset_z;
    int16_t gyro_offset_x;
    int16_t gyro_offset_y;
    int16_t gyro_offset_z;

    bool is_calibrated;
} imu_filter_t;

/**
 * @brief Initialize IMU complementary filter with default alpha and dt.
 */
void imu_filter_init(imu_filter_t *f, float alpha, float dt);

/**
 * @brief Set complementary filter weight alpha (0.0f < alpha < 1.0f).
 */
bool imu_filter_set_alpha(imu_filter_t *f, float alpha);

/**
 * @brief Update filter with raw MPU6050 accelerometer and gyroscope readings.
 * @param f Pointer to filter instance
 * @param ax Raw accel X (LSB)
 * @param ay Raw accel Y (LSB)
 * @param az Raw accel Z (LSB)
 * @param gx Raw gyro X (LSB)
 * @param gy Raw gyro Y (LSB)
 * @param gz Raw gyro Z (LSB)
 */
void imu_filter_update(imu_filter_t *f,
                       int16_t ax, int16_t ay, int16_t az,
                       int16_t gx, int16_t gy, int16_t gz);

/**
 * @brief Reset angle states to 0.0.
 */
void imu_filter_reset(imu_filter_t *f);

/**
 * @brief Apply calibration offsets.
 */
void imu_filter_set_offsets(imu_filter_t *f,
                            int16_t ax_off, int16_t ay_off, int16_t az_off,
                            int16_t gx_off, int16_t gy_off, int16_t gz_off);

#ifdef __cplusplus
}
#endif

#endif /* FILTER_H */
