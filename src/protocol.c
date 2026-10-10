#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

uint16_t protocol_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

bool protocol_encode_telemetry(uint8_t *buf, size_t buf_len,
                               uint8_t seq,
                               float pitch, float roll, float yaw,
                               uint16_t status_flags) {
    if (!buf || buf_len < sizeof(telemetry_frame_t)) {
        return false;
    }

    telemetry_frame_t *frame = (telemetry_frame_t *)buf;
    frame->magic0 = TELEMETRY_MAGIC_0;
    frame->magic1 = TELEMETRY_MAGIC_1;
    frame->len = (uint8_t)sizeof(telemetry_frame_t);
    frame->seq = seq;
    frame->type = TELEMETRY_TYPE_ANGLE;
    frame->pitch_x100 = (int16_t)(pitch * 100.0f);
    frame->roll_x100 = (int16_t)(roll * 100.0f);
    frame->yaw_x100 = (int16_t)(yaw * 100.0f);
    frame->status_flag = status_flags;

    /* Compute CRC over first 13 bytes (excluding CRC field itself) */
    frame->crc16 = protocol_crc16(buf, sizeof(telemetry_frame_t) - sizeof(uint16_t));
    return true;
}

bool protocol_decode_telemetry(const uint8_t *buf, size_t buf_len,
                               telemetry_frame_t *frame_out) {
    if (!buf || buf_len < sizeof(telemetry_frame_t) || !frame_out) {
        return false;
    }

    if (buf[0] != TELEMETRY_MAGIC_0 || buf[1] != TELEMETRY_MAGIC_1) {
        return false;
    }

    if (buf[2] != sizeof(telemetry_frame_t)) {
        return false;
    }

    uint16_t expected_crc = protocol_crc16(buf, sizeof(telemetry_frame_t) - sizeof(uint16_t));
    const telemetry_frame_t *raw_frame = (const telemetry_frame_t *)buf;

    if (raw_frame->crc16 != expected_crc) {
        return false;
    }

    memcpy(frame_out, buf, sizeof(telemetry_frame_t));
    return true;
}

static const char* skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

ascii_cmd_t protocol_parse_ascii_cmd(const char *cmd_line) {
    ascii_cmd_t res;
    res.type = CMD_NONE;
    res.param_float = 0.0f;
    res.param_int = 0;

    if (!cmd_line) return res;

    const char *s = skip_whitespace(cmd_line);
    if (*s == '\0') return res;

    if (strncmp(s, "CALIBRATE_IMU", 13) == 0) {
        res.type = CMD_CALIBRATE_IMU;
    } else if (strncmp(s, "SET_FILTER_ALPHA", 16) == 0) {
        res.type = CMD_SET_FILTER_ALPHA;
        const char *p = s + 16;
        while (*p && (*p == ':' || *p == '=' || isspace((unsigned char)*p))) p++;
        if (*p) {
            res.param_float = (float)atof(p);
        } else {
            res.param_float = 0.98f; /* default fallback */
        }
    } else if (strncmp(s, "GET_ANGLE", 9) == 0) {
        res.type = CMD_GET_ANGLE;
    } else if (strncmp(s, "SAVE_FLASH", 10) == 0) {
        res.type = CMD_SAVE_FLASH;
    } else if (strncmp(s, "LOAD_FLASH", 10) == 0) {
        res.type = CMD_LOAD_FLASH;
    } else if (strncmp(s, "DUMP_FLASH", 10) == 0) {
        res.type = CMD_DUMP_FLASH;
    } else {
        res.type = CMD_UNKNOWN;
    }

    return res;
}
