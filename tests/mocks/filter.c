#include "filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define RAD_TO_DEG (180.0f / (float)M_PI)

/*
 * ============================================================================
 * MPU6050 Complementary Filter & Angle Estimation Engine
 * Datasheet Reference: InvenSense RM-MPU-6000A Rev 4.0
 * Standard: DIN 9300 / ISO 1151 Aerospace Coordinate Systems
 * ============================================================================
 * Hardware Registers & Sensitivity Provenance:
 *   - Register 0x19 (SMPLRT_DIV, bits [7:0]): Sample Rate Divider (100 Hz output)
 *   - Register 0x1A (CONFIG, bits [2:0] DLPF_CFG): Digital Low Pass Filter
 *   - Register 0x1B (GYRO_CONFIG, bits [4:3] FS_SEL = 00b: +/-250 deg/s range)
 *     Sensitivity Scale Factor: 131.0 LSB / (deg/s) (RM-MPU-6000A Section 4.19)
 *   - Register 0x1C (ACCEL_CONFIG, bits [4:3] AFS_SEL = 00b: +/-2g range)
 *     Sensitivity Scale Factor: 16384.0 LSB/g (RM-MPU-6000A Section 4.17)
 *   - Accelerometer Output Registers:
 *     0x3B (ACCEL_XOUT_H), 0x3C (ACCEL_XOUT_L) -> 16-bit signed integer
 *     0x3D (ACCEL_YOUT_H), 0x3E (ACCEL_YOUT_L) -> 16-bit signed integer
 *     0x3F (ACCEL_ZOUT_H), 0x40 (ACCEL_ZOUT_L) -> 16-bit signed integer
 *   - Gyroscope Output Registers:
 *     0x43 (GYRO_XOUT_H),  0x44 (GYRO_XOUT_L)  -> 16-bit signed integer
 *     0x45 (GYRO_YOUT_H),  0x46 (GYRO_YOUT_L)  -> 16-bit signed integer
 *     0x47 (GYRO_ZOUT_H),  0x48 (GYRO_ZOUT_L)  -> 16-bit signed integer
 *
 * Coordinate Convention (DIN 9300 / ISO 1151):
 *   - Roll  (phi):   Rotation around X-axis (Right wing down = positive)
 *                    Angular rate p = Gyro X; Accel angle = atan2(Ay, Az)
 *   - Pitch (theta): Rotation around Y-axis (Nose up = positive)
 *                    Angular rate q = Gyro Y; Accel angle = atan2(-Ax, sqrt(Ay^2 + Az^2))
 *   - Yaw   (psi):   Rotation around Z-axis (Nose right = positive)
 *                    Angular rate r = Gyro Z
 * ============================================================================
 */
#define ACCEL_SCALE_FACTOR 16384.0f  /* LSB/g for +/-2g range (Register 0x1C AFS_SEL=00b) */
#define GYRO_SCALE_FACTOR  131.0f    /* LSB / (deg/s) for +/-250 dps (Register 0x1B FS_SEL=00b) */

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

    /*
     * Angular rates around body axes (DIN 9300):
     *   - Roll rate  (p): Gyro X (Register 0x43/0x44)
     *   - Pitch rate (q): Gyro Y (Register 0x45/0x46)
     *   - Yaw rate   (r): Gyro Z (Register 0x47/0x48)
     */
    float rate_roll  = (float)cal_gx / GYRO_SCALE_FACTOR; /* deg/s around X axis */
    float rate_pitch = (float)cal_gy / GYRO_SCALE_FACTOR; /* deg/s around Y axis */
    float rate_yaw   = (float)cal_gz / GYRO_SCALE_FACTOR; /* deg/s around Z axis */

    /*
     * Accelerometer tilt estimation (DIN 9300 / ISO 1151):
     *   - Roll  (phi):   atan2(Ay, Az)
     *   - Pitch (theta): atan2(-Ax, sqrt(Ay^2 + Az^2))
     */
    float roll_accel  = atan2f(fay, faz) * RAD_TO_DEG;
    float pitch_accel = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * RAD_TO_DEG;

    /*
     * Complementary Filter Fusion:
     * High-pass on integrated gyro rate + Low-pass on accelerometer inclination
     */
    f->roll  = f->alpha * (f->roll  + rate_roll  * f->dt) + (1.0f - f->alpha) * roll_accel;
    f->pitch = f->alpha * (f->pitch + rate_pitch * f->dt) + (1.0f - f->alpha) * pitch_accel;
    f->yaw  += rate_yaw * f->dt;
}
