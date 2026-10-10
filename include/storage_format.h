#ifndef STORAGE_FORMAT_H
#define STORAGE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_CONFIG_MAGIC 0xAA55BEEFul
#define FLASH_CONFIG_VERSION 1
#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE   256

#define FLASH_CONFIG_SECTOR_ADDR 0x00000000ul
#define FLASH_LOG_START_ADDR     0x00001000ul /* Sector 1 */
#define FLASH_LOG_END_ADDR       0x00010000ul /* Sector 16 (64 KB boundary) */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;          /* 0xAA55BEEF */
    uint16_t version;        /* 1 */
    uint16_t reserved;
    float    alpha;          /* Filter alpha, e.g. 0.98 */
    int16_t  accel_offset_x;
    int16_t  accel_offset_y;
    int16_t  accel_offset_z;
    int16_t  gyro_offset_x;
    int16_t  gyro_offset_y;
    int16_t  gyro_offset_z;
    uint32_t write_count;    /* Wear tracking counter */
    uint32_t crc32;          /* CRC32 of fields preceding crc32 */
} flash_config_block_t;

typedef struct {
    uint32_t timestamp_ms;
    int16_t  pitch_x100;
    int16_t  roll_x100;
    int16_t  yaw_x100;
    int16_t  temp_c_x10;
    uint16_t status_flags;
    uint16_t checksum;       /* XOR sum or 16-bit CRC of entry */
} flash_log_entry_t;
#pragma pack(pop)

/**
 * @brief Standard IEEE 802.3 CRC32 (poly 0xEDB88320).
 */
uint32_t storage_crc32(const uint8_t *data, size_t len);

/**
 * @brief Validate flash configuration block integrity.
 */
bool storage_validate_config(const flash_config_block_t *cfg);

/**
 * @brief Compute and seal CRC32 into flash configuration block.
 */
void storage_seal_config(flash_config_block_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_FORMAT_H */
