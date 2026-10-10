#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "filter.h"
#include "protocol.h"
#include "storage_format.h"

static void test_filter_basic(void) {
    printf("[TEST] Running test_filter_basic...\n");
    imu_filter_t f;
    imu_filter_init(&f, 0.98f, 0.01f);
    assert(fabsf(f.alpha - 0.98f) < 1e-4);
    assert(fabsf(f.dt - 0.01f) < 1e-4);
    assert(fabsf(f.pitch) < 1e-4);
    assert(fabsf(f.roll) < 1e-4);

    /* Flat stationary test: 1g along Z axis (ax=0, ay=0, az=16384, gx=0, gy=0, gz=0) */
    for (int i = 0; i < 50; i++) {
        imu_filter_update(&f, 0, 0, 16384, 0, 0, 0);
    }
    assert(fabsf(f.pitch) < 0.1f);
    assert(fabsf(f.roll) < 0.1f);

    /* 45 degree pitch tilt test: ay = 16384 * sin(45 deg) = 11585, az = 11585 */
    imu_filter_init(&f, 0.5f, 0.01f);
    for (int i = 0; i < 100; i++) {
        imu_filter_update(&f, 0, 11585, 11585, 0, 0, 0);
    }
    assert(fabsf(f.pitch - 45.0f) < 1.0f);

    /* Test set_alpha bounds */
    assert(imu_filter_set_alpha(&f, 0.95f) == true);
    assert(fabsf(f.alpha - 0.95f) < 1e-4);
    assert(imu_filter_set_alpha(&f, -0.1f) == false);
    assert(imu_filter_set_alpha(&f, 1.5f) == false);

    printf("[PASS] test_filter_basic passed.\n");
}

static void test_protocol_telemetry(void) {
    printf("[TEST] Running test_protocol_telemetry...\n");
    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));

    bool enc = protocol_encode_telemetry(buf, sizeof(buf), 42, 12.34f, -5.67f, 180.0f, 0x0003);
    assert(enc == true);
    assert(buf[0] == 0xAA);
    assert(buf[1] == 0x55);
    assert(buf[2] == 15);
    assert(buf[3] == 42);

    telemetry_frame_t frame;
    bool dec = protocol_decode_telemetry(buf, sizeof(buf), &frame);
    assert(dec == true);
    assert(frame.seq == 42);
    assert(frame.pitch_x100 == 1234);
    assert(frame.roll_x100 == -567);
    assert(frame.yaw_x100 == 18000);
    assert(frame.status_flag == 0x0003);

    /* Corrupt CRC */
    buf[14] ^= 0xFF;
    dec = protocol_decode_telemetry(buf, sizeof(buf), &frame);
    assert(dec == false);

    /* Corrupt Magic */
    buf[14] ^= 0xFF; /* restore CRC */
    buf[0] = 0x00;
    dec = protocol_decode_telemetry(buf, sizeof(buf), &frame);
    assert(dec == false);

    printf("[PASS] test_protocol_telemetry passed.\n");
}

static void test_protocol_ascii(void) {
    printf("[TEST] Running test_protocol_ascii...\n");

    ascii_cmd_t cmd = protocol_parse_ascii_cmd("CALIBRATE_IMU\r\n");
    assert(cmd.type == CMD_CALIBRATE_IMU);

    cmd = protocol_parse_ascii_cmd("SET_FILTER_ALPHA:0.95\n");
    assert(cmd.type == CMD_SET_FILTER_ALPHA);
    assert(fabsf(cmd.param_float - 0.95f) < 1e-4);

    cmd = protocol_parse_ascii_cmd("GET_ANGLE\r\n");
    assert(cmd.type == CMD_GET_ANGLE);

    cmd = protocol_parse_ascii_cmd("SAVE_FLASH\n");
    assert(cmd.type == CMD_SAVE_FLASH);

    cmd = protocol_parse_ascii_cmd("LOAD_FLASH\n");
    assert(cmd.type == CMD_LOAD_FLASH);

    cmd = protocol_parse_ascii_cmd("INVALID_CMD_XYZ\n");
    assert(cmd.type == CMD_UNKNOWN);

    cmd = protocol_parse_ascii_cmd("   \r\n");
    assert(cmd.type == CMD_NONE);

    printf("[PASS] test_protocol_ascii passed.\n");
}

static void test_storage_config(void) {
    printf("[TEST] Running test_storage_config...\n");

    flash_config_block_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.alpha = 0.98f;
    cfg.accel_offset_x = 120;
    cfg.accel_offset_y = -85;
    cfg.accel_offset_z = 240;
    cfg.gyro_offset_x = 15;
    cfg.gyro_offset_y = -30;
    cfg.gyro_offset_z = 8;
    cfg.write_count = 1;

    storage_seal_config(&cfg);
    assert(cfg.magic == FLASH_CONFIG_MAGIC);
    assert(cfg.version == FLASH_CONFIG_VERSION);

    assert(storage_validate_config(&cfg) == true);

    /* Corrupt one field */
    cfg.alpha = 0.50f;
    assert(storage_validate_config(&cfg) == false);

    printf("[PASS] test_storage_config passed.\n");
}

int main(void) {
    printf("=== RUNNING FIRMWARE CORE UNIT TESTS ===\n");
    test_filter_basic();
    test_protocol_telemetry();
    test_protocol_ascii();
    test_storage_config();
    printf("=== ALL FIRMWARE TESTS PASSED (100%% SUCCESS) ===\n");
    return 0;
}
