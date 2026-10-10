#ifndef W25QXX_H
#define W25QXX_H

#include <stdint.h>
#include <stdbool.h>
#include "storage_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q64_JEDEC_ID 0xEF4017ul

/* W25Qxx Opcodes */
#define W25QXX_CMD_WRITE_ENABLE     0x06
#define W25QXX_CMD_WRITE_DISABLE    0x04
#define W25QXX_CMD_READ_STATUS_R1   0x05
#define W25QXX_CMD_PAGE_PROGRAM     0x02
#define W25QXX_CMD_SECTOR_ERASE_4K  0x20
#define W25QXX_CMD_READ_DATA        0x03
#define W25QXX_CMD_JEDEC_ID         0x9F

#define W25QXX_STATUS_BUSY          0x01
#define W25QXX_STATUS_WEL           0x02

/**
 * @brief Initialize SPI flash interface and verify JEDEC ID.
 */
bool w25qxx_init(void);

/**
 * @brief Read 3-byte JEDEC manufacturer and device ID.
 */
uint32_t w25qxx_read_jedec_id(void);

/**
 * @brief Erase a 4KB sector at the given byte address.
 */
bool w25qxx_erase_sector(uint32_t sector_addr);

/**
 * @brief Write up to 256 bytes to a page.
 */
bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len);

/**
 * @brief Read arbitrary length buffer from flash.
 */
bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief Save configuration block into Sector 0.
 */
bool w25qxx_save_config(const flash_config_block_t *cfg);

/**
 * @brief Load and validate configuration block from Sector 0.
 */
bool w25qxx_load_config(flash_config_block_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* W25QXX_H */
