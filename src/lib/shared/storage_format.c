#include "storage_format.h"

uint32_t storage_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFul;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320ul;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

bool storage_validate_config(const flash_config_block_t *cfg) {
    if (!cfg) return false;
    if (cfg->magic != FLASH_CONFIG_MAGIC) return false;
    if (cfg->version != FLASH_CONFIG_VERSION) return false;
    if (cfg->alpha <= 0.0f || cfg->alpha >= 1.0f) return false;

    size_t payload_len = sizeof(flash_config_block_t) - sizeof(uint32_t);
    uint32_t expected_crc = storage_crc32((const uint8_t *)cfg, payload_len);

    return (cfg->crc32 == expected_crc);
}

void storage_seal_config(flash_config_block_t *cfg) {
    if (!cfg) return;
    cfg->magic = FLASH_CONFIG_MAGIC;
    cfg->version = FLASH_CONFIG_VERSION;
    size_t payload_len = sizeof(flash_config_block_t) - sizeof(uint32_t);
    cfg->crc32 = storage_crc32((const uint8_t *)cfg, payload_len);
}
