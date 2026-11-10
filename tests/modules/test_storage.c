#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "storage_format.h"

int test_storage_module(void) {
    int failed = 0;

    printf("  [RUN] test_storage_struct_size\n");
    if (sizeof(flash_config_block_t) != 32) failed++;

    printf("  [RUN] test_storage_crc32\n");
    const uint8_t data[] = "123456789";
    uint32_t crc = storage_crc32(data, 9);
    /* Standard IEEE 802.3 CRC32 of "123456789" = 0xCBF43926 */
    if (crc != 0xCBF43926) failed++;

    printf("  [RUN] test_storage_defaults_and_seal\n");
    flash_config_block_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = FLASH_CONFIG_MAGIC;
    cfg.version = FLASH_CONFIG_VERSION;
    cfg.alpha = 0.98f;
    cfg.write_count = 1;
    storage_seal_config(&cfg);

    if (cfg.magic != FLASH_CONFIG_MAGIC) failed++;
    if (cfg.version != FLASH_CONFIG_VERSION) failed++;
    if (fabsf(cfg.alpha - 0.98f) >= 1e-4) failed++;
    if (cfg.accel_offset_x != 0) failed++;
    if (cfg.accel_offset_y != 0) failed++;
    if (cfg.accel_offset_z != 0) failed++;
    if (storage_validate_config(&cfg) != true) failed++;

    printf("  [RUN] test_storage_custom_seal_and_validate\n");
    memset(&cfg, 0, sizeof(cfg));
    cfg.alpha = 0.92f;
    cfg.accel_offset_x = -150;
    cfg.accel_offset_y = 75;
    cfg.accel_offset_z = 210;
    cfg.gyro_offset_x = -12;
    cfg.gyro_offset_y = 34;
    cfg.gyro_offset_z = 5;
    cfg.write_count = 42;

    storage_seal_config(&cfg);
    if (cfg.magic != FLASH_CONFIG_MAGIC) failed++;
    if (cfg.version != FLASH_CONFIG_VERSION) failed++;
    if (storage_validate_config(&cfg) != true) failed++;

    printf("  [RUN] test_storage_corruption_rejection\n");
    /* Corrupted Magic */
    cfg.magic = 0x12345678;
    if (storage_validate_config(&cfg) != false) failed++;
    cfg.magic = FLASH_CONFIG_MAGIC;

    /* Corrupted Version */
    cfg.version = 99;
    if (storage_validate_config(&cfg) != false) failed++;
    cfg.version = FLASH_CONFIG_VERSION;

    /* Corrupted Alpha */
    cfg.alpha = 0.95f;
    if (storage_validate_config(&cfg) != false) failed++;
    cfg.alpha = 0.92f;

    /* Corrupted Offset */
    cfg.accel_offset_x = 0;
    if (storage_validate_config(&cfg) != false) failed++;
    cfg.accel_offset_x = -150;
    if (storage_validate_config(&cfg) != true) failed++;

    /* Corrupted CRC field */
    cfg.crc32 ^= 0x00000001;
    if (storage_validate_config(&cfg) != false) failed++;

    /* Null pointer safety */
    if (storage_validate_config(NULL) != false) failed++;
    storage_seal_config(NULL);

    if (failed == 0) {
        printf("  [PASS] test_storage_module (0 failures)\n");
    } else {
        fprintf(stderr, "  [FAIL] test_storage_module (%d failures)\n", failed);
    }
    return failed;
}
