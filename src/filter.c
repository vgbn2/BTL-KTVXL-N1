#include "filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define RAD_TO_DEG (180.0f / (float)M_PI)

/* MPU6050 Scale Factors (default +/-2g, +/-250 deg/s) */
#define ACCEL_SCALE_FACTOR 16384.0f  /* LSB/g */
#define GYRO_SCALE_FACTOR  131.0f    /* LSB / (deg/s) */

void imu_filter_init(imu_filter_t *f, float alpha, float dt) {
    if (!f) return;
    f->alpha = (alpha > 0.0f && alpha < 1.0f) ? alpha : 0.98f;
    f->dt = (dt > 0.0f) ? dt : 0.01f;
    f->pitch = 0.0f;
    f->roll = 0.0f;
    f->yaw = 0.0f;
    f->accel_offset_x = 0;
    f->accel_offset_y = 0;
    f->accel_offset_z = 0;
    f->gyro_offset_x = 0;
    f->gyro_offset_y = 0;
    f->gyro_offset_z = 0;
    f->is_calibrated = false;
}

bool imu_filter_set_alpha(imu_filter_t *f, float alpha) {
    if (!f || alpha <= 0.0f || alpha >= 1.0f) {
        return false;
    }
    f->alpha = alpha;
    return true;
}

void imu_filter_set_offsets(imu_filter_t *f,
                            int16_t ax_off, int16_t ay_off, int16_t az_off,
                            int16_t gx_off, int16_t gy_off, int16_t gz_off) {
    if (!f) return;
    f->accel_offset_x = ax_off;
    f->accel_offset_y = ay_off;
    f->accel_offset_z = az_off;
    f->gyro_offset_x = gx_off;
    f->gyro_offset_y = gy_off;
    f->gyro_offset_z = gz_off;
    f->is_calibrated = true;
}

void imu_filter_reset(imu_filter_t *f) {
    if (!f) return;
    f->pitch = 0.0f;
    f->roll = 0.0f;
    f->yaw = 0.0f;
}

void imu_filter_update(imu_filter_t *f,
                       int16_t ax, int16_t ay, int16_t az,
                       int16_t gx, int16_t gy, int16_t gz) {
    if (!f) return;

    /* Subtract calibration offsets */
    int32_t cal_ax = (int32_t)ax - f->accel_offset_x;
    int32_t cal_ay = (int32_t)ay - f->accel_offset_y;
    int32_t cal_az = (int32_t)az - f->accel_offset_z;
    int32_t cal_gx = (int32_t)gx - f->gyro_offset_x;
    int32_t cal_gy = (int32_t)gy - f->gyro_offset_y;
    int32_t cal_gz = (int32_t)gz - f->gyro_offset_z;

    /* Convert to physical units */
    float fax = (float)cal_ax / ACCEL_SCALE_FACTOR;
    float fay = (float)cal_ay / ACCEL_SCALE_FACTOR;
    float faz = (float)cal_az / ACCEL_SCALE_FACTOR;

    float rate_pitch = (float)cal_gx / GYRO_SCALE_FACTOR; /* deg/s */
    float rate_roll  = (float)cal_gy / GYRO_SCALE_FACTOR; /* deg/s */
    float rate_yaw   = (float)cal_gz / GYRO_SCALE_FACTOR; /* deg/s */

    /* Calculate tilt angles from accelerometer */
    /* Pitch: rotation around X axis (Y vs Z) */
    float pitch_accel = atan2f(fay, sqrtf(fax * fax + faz * faz)) * RAD_TO_DEG;
    /* Roll: rotation around Y axis (-X vs Z) */
    float roll_accel  = atan2f(-fax, faz) * RAD_TO_DEG;

    /* Complementary Filter Fusion */
    f->pitch = f->alpha * (f->pitch + rate_pitch * f->dt) + (1.0f - f->alpha) * pitch_accel;
    f->roll  = f->alpha * (f->roll  + rate_roll  * f->dt) + (1.0f - f->alpha) * roll_accel;
    f->yaw  += rate_yaw * f->dt;
}
