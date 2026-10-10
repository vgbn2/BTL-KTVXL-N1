#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEMETRY_MAGIC_0 0xAA
#define TELEMETRY_MAGIC_1 0x55
#define TELEMETRY_FRAME_LEN 15
#define TELEMETRY_TYPE_ANGLE 0x01

/* Packed binary telemetry packet (15 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t magic0;       /* 0xAA */
    uint8_t magic1;       /* 0x55 */
    uint8_t len;          /* 15 */
    uint8_t seq;          /* Frame sequence counter */
    uint8_t type;         /* 0x01 = Angle Telemetry */
    int16_t pitch_x100;   /* Pitch in deg * 100 */
    int16_t roll_x100;    /* Roll in deg * 100 */
    int16_t yaw_x100;     /* Yaw in deg * 100 */
    uint16_t status_flag; /* Bit 0: calibrated, Bit 1: flash_ok */
    uint16_t crc16;       /* CRC16-CCITT across bytes 0..12 */
} telemetry_frame_t;
#pragma pack(pop)

typedef enum {
    CMD_NONE = 0,
    CMD_CALIBRATE_IMU,
    CMD_SET_FILTER_ALPHA,
    CMD_GET_ANGLE,
    CMD_SAVE_FLASH,
    CMD_LOAD_FLASH,
    CMD_DUMP_FLASH,
    CMD_UNKNOWN
} ascii_cmd_type_t;

typedef struct {
    ascii_cmd_type_t type;
    float param_float;
    int32_t param_int;
} ascii_cmd_t;

/**
 * @brief Calculate CRC16-CCITT (poly 0x1021, init 0xFFFF).
 */
uint16_t protocol_crc16(const uint8_t *data, size_t len);

/**
 * @brief Pack an angle telemetry frame into buffer. Returns true on success.
 */
bool protocol_encode_telemetry(uint8_t *buf, size_t buf_len,
                               uint8_t seq,
                               float pitch, float roll, float yaw,
                               uint16_t status_flags);

/**
 * @brief Decode binary telemetry frame from buffer. Returns true if valid.
 */
bool protocol_decode_telemetry(const uint8_t *buf, size_t buf_len,
                               telemetry_frame_t *frame_out);

/**
 * @brief Parse incoming ASCII command line (null-terminated).
 */
ascii_cmd_t protocol_parse_ascii_cmd(const char *cmd_line);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
