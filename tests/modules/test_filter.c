#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "filter.h"

int test_filter_module(void) {
    int failed = 0;

    printf("  [RUN] test_filter_init_and_bounds\n");
    imu_filter_t f;
    imu_filter_init(&f, 0.98f, 0.01f);
    if (fabsf(f.alpha - 0.98f) >= 1e-4) failed++;
    if (fabsf(f.dt - 0.01f) >= 1e-4) failed++;
    if (fabsf(f.pitch) >= 1e-4) failed++;
    if (fabsf(f.roll) >= 1e-4) failed++;
    if (fabsf(f.yaw) >= 1e-4) failed++;
    if (f.is_calibrated) failed++;

    /* Alpha bounds check */
    if (imu_filter_set_alpha(&f, 0.95f) != true) failed++;
    if (fabsf(f.alpha - 0.95f) >= 1e-4) failed++;
    if (imu_filter_set_alpha(&f, -0.01f) != false) failed++;
    if (imu_filter_set_alpha(&f, 1.05f) != false) failed++;
    if (imu_filter_set_alpha(NULL, 0.5f) != false) failed++;

    printf("  [RUN] test_filter_flat_stationary\n");
    imu_filter_init(&f, 0.98f, 0.01f);
    /* 1g on Z axis: ax=0, ay=0, az=16384 (accel scale +-2g -> 16384 LSB/g) */
    for (int i = 0; i < 100; i++) {
        imu_filter_update(&f, 0, 0, 16384, 0, 0, 0);
    }
    if (fabsf(f.pitch) >= 0.05f) failed++;
    if (fabsf(f.roll) >= 0.05f) failed++;

    printf("  [RUN] test_filter_tilt_convergence (DIN 9300)\n");
    /* 45 degree roll tilt (X-axis rotation): ay = 16384 * sin(45 deg) = 11585, az = 11585 */
    imu_filter_init(&f, 0.50f, 0.01f);
    for (int i = 0; i < 100; i++) {
        imu_filter_update(&f, 0, 11585, 11585, 0, 0, 0);
    }
    if (fabsf(f.roll - 45.0f) >= 1.0f) failed++;

    /* 30 degree pitch tilt (Y-axis rotation): ax = -16384 * sin(30 deg) = -8192, az = 16384 * cos(30 deg) = 14189 */
    imu_filter_init(&f, 0.50f, 0.01f);
    for (int i = 0; i < 100; i++) {
        imu_filter_update(&f, -8192, 0, 14189, 0, 0, 0);
    }
    if (fabsf(f.pitch - 30.0f) >= 1.0f) failed++;

    printf("  [RUN] test_filter_offsets_and_calibration\n");
    imu_filter_init(&f, 0.98f, 0.01f);
    imu_filter_set_offsets(&f, 100, -100, 200, 50, -50, 25);
    if (!f.is_calibrated) failed++;
    if (f.accel_offset_x != 100) failed++;
    if (f.accel_offset_y != -100) failed++;
    if (f.accel_offset_z != 200) failed++;

    /* Apply raw readings matching offsets -> effective reading is 0 except Z */
    for (int i = 0; i < 50; i++) {
        imu_filter_update(&f, 100, -100, 16384 + 200, 50, -50, 25);
    }
    if (fabsf(f.pitch) >= 0.1f) failed++;
    if (fabsf(f.roll) >= 0.1f) failed++;

    if (failed == 0) {
        printf("  [PASS] test_filter_module (0 failures)\n");
    } else {
        fprintf(stderr, "  [FAIL] test_filter_module (%d failures)\n", failed);
    }
    return failed;
}
