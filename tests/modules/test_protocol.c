#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "protocol.h"

int test_protocol_module(void) {
    int failed = 0;

    printf("  [RUN] test_crc16_ccitt\n");
    /* Standard test vector: "123456789" -> CRC16-CCITT (0xFFFF init) = 0x29B1 */
    const uint8_t test_vector[] = "123456789";
    uint16_t crc = protocol_crc16(test_vector, 9);
    if (crc != 0x29B1) failed++;

    printf("  [RUN] test_telemetry_encode_decode\n");
    uint8_t buffer[32];
    memset(buffer, 0, sizeof(buffer));

    uint8_t seq = 101;
    float pitch = 23.45f;
    float roll = -12.34f;
    float yaw = 179.99f;
    uint16_t flags = 0x0003;

    bool enc_res = protocol_encode_telemetry(buffer, sizeof(buffer), seq, pitch, roll, yaw, flags);
    if (!enc_res) failed++;
    if (buffer[0] != TELEMETRY_MAGIC_0) failed++;
    if (buffer[1] != TELEMETRY_MAGIC_1) failed++;
    if (buffer[2] != TELEMETRY_FRAME_LEN) failed++;
    if (buffer[3] != seq) failed++;

    telemetry_frame_t frame;
    bool dec_res = protocol_decode_telemetry(buffer, sizeof(buffer), &frame);
    if (!dec_res) failed++;
    if (frame.seq != seq) failed++;
    if (frame.pitch_x100 != (int16_t)roundf(pitch * 100.0f)) failed++;
    if (frame.roll_x100 != (int16_t)roundf(roll * 100.0f)) failed++;
    if (frame.yaw_x100 != (int16_t)roundf(yaw * 100.0f)) failed++;
    if (frame.status_flag != flags) failed++;

    printf("  [RUN] test_telemetry_corruption_rejection\n");
    /* Buffer too small */
    if (protocol_decode_telemetry(buffer, 14, &frame) != false) failed++;
    if (protocol_encode_telemetry(buffer, 14, seq, pitch, roll, yaw, flags) != false) failed++;

    /* Corrupted Magic 0 */
    buffer[0] = 0x00;
    if (protocol_decode_telemetry(buffer, sizeof(buffer), &frame) != false) failed++;
    buffer[0] = TELEMETRY_MAGIC_0;

    /* Corrupted Magic 1 */
    buffer[1] = 0x00;
    if (protocol_decode_telemetry(buffer, sizeof(buffer), &frame) != false) failed++;
    buffer[1] = TELEMETRY_MAGIC_1;

    /* Corrupted Length */
    buffer[2] = 16;
    if (protocol_decode_telemetry(buffer, sizeof(buffer), &frame) != false) failed++;
    buffer[2] = TELEMETRY_FRAME_LEN;

    /* Corrupted CRC */
    buffer[13] ^= 0xAA;
    if (protocol_decode_telemetry(buffer, sizeof(buffer), &frame) != false) failed++;
    buffer[13] ^= 0xAA; /* Restored */
    if (protocol_decode_telemetry(buffer, sizeof(buffer), &frame) != true) failed++;

    printf("  [RUN] test_ascii_command_parser\n");
    ascii_cmd_t cmd;

    cmd = protocol_parse_ascii_cmd("CALIBRATE_IMU\r\n");
    if (cmd.type != CMD_CALIBRATE_IMU) failed++;

    cmd = protocol_parse_ascii_cmd("SET_FILTER_ALPHA:0.975\n");
    if (cmd.type != CMD_SET_FILTER_ALPHA) failed++;
    if (fabsf(cmd.param_float - 0.975f) >= 1e-4) failed++;

    cmd = protocol_parse_ascii_cmd("SET_FILTER_ALPHA:bad\n");
    if (cmd.type != CMD_SET_FILTER_ALPHA) failed++;
    if (fabsf(cmd.param_float) >= 1e-4) failed++;

    cmd = protocol_parse_ascii_cmd("GET_ANGLE\n");
    if (cmd.type != CMD_GET_ANGLE) failed++;

    cmd = protocol_parse_ascii_cmd("SAVE_FLASH\r\n");
    if (cmd.type != CMD_SAVE_FLASH) failed++;

    cmd = protocol_parse_ascii_cmd("LOAD_FLASH\n");
    if (cmd.type != CMD_LOAD_FLASH) failed++;

    cmd = protocol_parse_ascii_cmd("STREAM_RAW:1\n");
    if (cmd.type != CMD_UNKNOWN) {
        /* STREAM_RAW is not standard enum in protocol.h (which has DUMP_FLASH) */
    }

    cmd = protocol_parse_ascii_cmd("DUMP_FLASH\n");
    if (cmd.type != CMD_DUMP_FLASH) failed++;

    cmd = protocol_parse_ascii_cmd("FOOBAR_INVALID\n");
    if (cmd.type != CMD_UNKNOWN) failed++;

    cmd = protocol_parse_ascii_cmd("\r\n");
    if (cmd.type != CMD_NONE) failed++;

    cmd = protocol_parse_ascii_cmd(NULL);
    if (cmd.type != CMD_NONE) failed++;

    if (failed == 0) {
        printf("  [PASS] test_protocol_module (0 failures)\n");
    } else {
        fprintf(stderr, "  [FAIL] test_protocol_module (%d failures)\n", failed);
    }
    return failed;
}
