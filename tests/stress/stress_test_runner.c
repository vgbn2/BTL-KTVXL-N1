#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "protocol.h"
#include "storage_format.h"
#include "mpu6050.h"
#include "w25qxx.h"

#define STRESS_ITERATIONS       100000
#define FUZZ_ITERATIONS         10000
#define SATURATION_ITERATIONS   50000

/* Domain Kinematics & Protocol Constants */
#define ANGLE_CENTIDEGREE_SCALE 100.0f
#define PHYSICAL_MIN_ANGLE_DEG  -180.0f
#define PHYSICAL_MAX_ANGLE_DEG   180.0f
#define PHYSICAL_ANGLE_SPAN_DEG (PHYSICAL_MAX_ANGLE_DEG - PHYSICAL_MIN_ANGLE_DEG)

#define STATUS_FLAG_CALIBRATED  0x0001
#define STATUS_FLAG_FLASH_OK    0x0002

static float generate_domain_angle(int index, float phase_offset_deg) {
    /* Map index deterministically across the full physical [-180, +180] span */
    float norm = (float)(index % (int)(PHYSICAL_ANGLE_SPAN_DEG * 10)) / (PHYSICAL_ANGLE_SPAN_DEG * 10);
    float angle = PHYSICAL_MIN_ANGLE_DEG + (norm * PHYSICAL_ANGLE_SPAN_DEG) + phase_offset_deg;
    while (angle > PHYSICAL_MAX_ANGLE_DEG) angle -= PHYSICAL_ANGLE_SPAN_DEG;
    while (angle < PHYSICAL_MIN_ANGLE_DEG) angle += PHYSICAL_ANGLE_SPAN_DEG;
    return angle;
}

int main(void) {
    uint32_t total_checks = 0;
    uint32_t failed_checks = 0;

    printf("====================================================\n");
    printf("   STRESS & FUZZ TEST SUITE: STM32 ARCHITECTURE     \n");
    printf("====================================================\n");

    /* 1. High-Throughput Telemetry Codec Stress */
    printf("[STRESS 1/3] Running %d Telemetry Frame Encode/Decode Cycles...\n", STRESS_ITERATIONS);
    uint8_t buffer[TELEMETRY_FRAME_LEN];
    telemetry_frame_t frame;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        uint8_t seq = (uint8_t)(i % 256);
        float pitch = generate_domain_angle(i, 0.0f);
        float roll  = generate_domain_angle(i, 45.0f);
        float yaw   = generate_domain_angle(i, 90.0f);
        uint16_t flags = (i % 2 == 0 ? STATUS_FLAG_CALIBRATED : 0) |
                         (i % 3 == 0 ? STATUS_FLAG_FLASH_OK : 0);

        total_checks++;
        if (!protocol_encode_telemetry(buffer, sizeof(buffer), seq, pitch, roll, yaw, flags)) {
            failed_checks++;
            continue;
        }

        total_checks++;
        if (!protocol_decode_telemetry(buffer, sizeof(buffer), &frame)) {
            failed_checks++;
            continue;
        }

        total_checks++;
        int16_t expected_pitch = (int16_t)(pitch * ANGLE_CENTIDEGREE_SCALE);
        int16_t expected_roll  = (int16_t)(roll * ANGLE_CENTIDEGREE_SCALE);
        int16_t expected_yaw   = (int16_t)(yaw * ANGLE_CENTIDEGREE_SCALE);

        if (frame.seq != seq ||
            frame.pitch_x100 != expected_pitch ||
            frame.roll_x100 != expected_roll ||
            frame.yaw_x100 != expected_yaw ||
            frame.status_flag != flags) {
            failed_checks++;
        }
    }
    printf("  -> Codec stress completed: evaluated %u frame assertions.\n", total_checks);

    /* 2. Channel Noise & Bit-Flip Fuzzing */
    printf("[STRESS 2/3] Running %d Fuzzing Iterations (Random Bit Flips)...\n", FUZZ_ITERATIONS);
    uint32_t prng_state = 0x5A5A5A5A;
    #define PRNG_NEXT() (prng_state = prng_state * 1664525u + 1013904223u)

    uint32_t rejected_corrupted_frames = 0;
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        protocol_encode_telemetry(buffer, sizeof(buffer), (uint8_t)(i % 256), 12.34f, -45.67f, 90.0f, STATUS_FLAG_CALIBRATED);

        /* Inject 1 to 4 random bit flips across the frame */
        int flip_count = 1 + (int)(PRNG_NEXT() % 4);
        for (int f = 0; f < flip_count; f++) {
            size_t byte_idx = PRNG_NEXT() % TELEMETRY_FRAME_LEN;
            uint8_t bit_mask = (uint8_t)(1u << (PRNG_NEXT() % 8));
            buffer[byte_idx] ^= bit_mask;
        }

        /* Parser must detect corruption or reject invalid magic/CRC without memory fault */
        total_checks++;
        if (!protocol_decode_telemetry(buffer, sizeof(buffer), &frame)) {
            rejected_corrupted_frames++;
        }
    }
    printf("  -> Fuzz test completed: %u / %d corrupted frames caught (%.2f%% detection rate).\n",
           rejected_corrupted_frames, FUZZ_ITERATIONS, (float)rejected_corrupted_frames * 100.0f / FUZZ_ITERATIONS);

    /* Minimum statistical corruption detection rate for 1-4 bit flips with CRC16 */
    total_checks++;
    if (rejected_corrupted_frames < (uint32_t)(FUZZ_ITERATIONS * 0.999f)) {
        failed_checks++;
    }

    /* 3. Extreme Sensor Input / Saturation Stress */
    printf("[STRESS 3/3] Sensor Numerical Stability & Saturation Stress (%d pulses)...\n", SATURATION_ITERATIONS);
    imu_filter_t filter;
    imu_filter_init(&filter, 0.98f, 0.01f);

    for (int i = 0; i < SATURATION_ITERATIONS; i++) {
        /* Stress test with alternating extreme ADC limits (INT16_MIN, INT16_MAX, 0) */
        int16_t ax = (int16_t)((i % 2 == 0) ? INT16_MAX : INT16_MIN);
        int16_t ay = (int16_t)((i % 3 == 0) ? INT16_MIN : INT16_MAX);
        int16_t az = (int16_t)((i % 5 == 0) ? 0 : 16384);
        int16_t gx = (int16_t)((i % 7 == 0) ? INT16_MAX : INT16_MIN);
        int16_t gy = (int16_t)((i % 11 == 0) ? INT16_MIN : INT16_MAX);
        int16_t gz = (int16_t)((i % 13 == 0) ? 0 : (INT16_MAX / 2));

        imu_filter_update(&filter, ax, ay, az, gx, gy, gz);

        total_checks++;
        if (isnan(filter.pitch) || isinf(filter.pitch) ||
            isnan(filter.roll) || isinf(filter.roll) ||
            isnan(filter.yaw) || isinf(filter.yaw)) {
            failed_checks++;
            break;
        }
    }

    /* Final Conditional Evaluation - never hardcode pass output */
    printf("\n====================================================\n");
    if (failed_checks == 0) {
        printf("   ALL STRESS & FUZZING SUITES PASSED (%u checks) ✅\n", total_checks);
        printf("====================================================\n");
        return 0;
    } else {
        fprintf(stderr, "   STRESS & FUZZING SUITES FAILED (%u/%u failed) ❌\n", failed_checks, total_checks);
        fprintf(stderr, "====================================================\n");
        return 1;
    }
}
