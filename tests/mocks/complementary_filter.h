#ifndef COMPLEMENTARY_FILTER_H
#define COMPLEMENTARY_FILTER_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ANGLE_RAD_TO_DEG
#define ANGLE_RAD_TO_DEG 57.2957795f
#endif

static inline void ComplementaryFilter_Init(float alpha)
{
    (void)alpha;
}

static inline void ComplementaryFilter_Update(float ax, float ay, float az,
                                             float gx, float gy, float gz,
                                             float dt, float alpha,
                                             float *out_roll, float *out_pitch)
{
    (void)gz;
    float acc_roll = atan2f(ay, az) * ANGLE_RAD_TO_DEG;
    float acc_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * ANGLE_RAD_TO_DEG;

    /* Gyro scale: 131 LSB/(deg/s) for +/-250 dps */
    float gx_dps = gx / 131.0f;
    float gy_dps = gy / 131.0f;

    if (out_roll != (void*)0) {
        *out_roll = alpha * (*out_roll + gx_dps * dt) + (1.0f - alpha) * acc_roll;
    }
    if (out_pitch != (void*)0) {
        *out_pitch = alpha * (*out_pitch + gy_dps * dt) + (1.0f - alpha) * acc_pitch;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* COMPLEMENTARY_FILTER_H */
