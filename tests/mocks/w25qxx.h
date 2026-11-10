#ifndef W25QXX_H
#define W25QXX_H

#include <stdint.h>
#include <stdbool.h>
#include "imu_types.h"
#include "storage_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * Winbond W25Q64FV Serial NOR Flash Memory Driver (64M-Bit / 8M-Byte)
 * Datasheet Reference: Winbond W25Q64FV Rev J (06/03/2013)
 * SPI Bus: Standard SPI Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1)
 * ============================================================================
 * Memory Architecture & Geometry:
 *   - Total Capacity: 8,388,608 bytes (8 MB)
 *   - 128 Blocks (64 KB each)
 *   - 2,048 Sectors (4 KB each) - Minimum Erase Unit
 *   - 32,768 Pages (256 bytes each) - Maximum Write Unit
 *
 * Status Register 1 (Opcode 0x05 - Read Status Register-1):
 *   - Bit 0 BUSY: Erase/Write in Progress (1 = Device is busy, 0 = Standby)
 *   - Bit 1 WEL:  Write Enable Latch (1 = Write enabled, 0 = Write disabled)
 *   - Bit 2 BP0:  Block Protect 0 (Software write protection)
 *   - Bit 3 BP1:  Block Protect 1
 *   - Bit 4 BP2:  Block Protect 2
 *   - Bit 5 TB:   Top/Bottom Protect (0 = Top, 1 = Bottom)
 *   - Bit 6 SEC:  Sector Protect (0 = 64KB block, 1 = 4KB sector)
 *   - Bit 7 SRP0: Status Register Protect 0
 *
 * Status Register 2 (Opcode 0x35 - Read Status Register-2):
 *   - Bit 0 SRP1: Status Register Protect 1
 *   - Bit 1 QE:   Quad Enable (0 = Standard/Dual SPI, 1 = Quad SPI enabled)
 *   - Bit 7 SUS:  Suspend Status
 *
 * Instruction Opcodes (Section 7.2 Table 5):
 *   - 0x06: Write Enable (Sets WEL bit prior to write/erase operations)
 *   - 0x04: Write Disable (Clears WEL bit)
 *   - 0x05: Read Status Register-1 (Returns BUSY, WEL, BP bits)
 *   - 0x35: Read Status Register-2 (Returns QE, SUS bits)
 *   - 0x01: Write Status Register
 *   - 0x03: Read Data (Standard SPI, up to 50 MHz)
 *   - 0x0B: Fast Read (Requires 1 dummy byte, up to 104 MHz)
 *   - 0x02: Page Program (Writes 1 to 256 bytes; wraps if addr + len > 256)
 *   - 0x20: Sector Erase 4KB (Sets all bits to 0xFF, typical 45ms, max 400ms)
 *   - 0x52: Block Erase 32KB
 *   - 0xD8: Block Erase 64KB (Typical 150ms)
 *   - 0xC7: Chip Erase (Erases entire memory)
 *   - 0x9F: Read JEDEC ID (Returns 3 bytes: 0xEF, 0x40, 0x17)
 * ============================================================================
 */

/* Dia chi luu cau hinh tren W25Qxx (can theo sector 4KB) */
#define FLASH_CONFIG_BASE_ADDR   0x000000UL /* Sector 0: 0x000000 - 0x000FFF (4096 bytes) */
#define FLASH_DATA_LOG_ADDR      0x001000UL /* Sector 1: 0x001000 - 0x001FFF (Data telemetry logging) */

/* Winbond W25Q64 JEDEC ID */
#define W25Q64_JEDEC_ID          0xEF4017ul

/* Lenh chuan W25Qxx (TV4 / Google Doc naming) */
#define W25Q_CMD_WRITE_ENABLE    0x06
#define W25Q_CMD_WRITE_DISABLE   0x04
#define W25Q_CMD_READ_STATUS1    0x05
#define W25Q_CMD_READ_STATUS2    0x35
#define W25Q_CMD_READ_DATA       0x03
#define W25Q_CMD_PAGE_PROGRAM    0x02
#define W25Q_CMD_SECTOR_ERASE    0x20
#define W25Q_CMD_BLOCK_ERASE_64K 0xD8
#define W25Q_CMD_CHIP_ERASE      0xC7
#define W25Q_CMD_JEDEC_ID        0x9F

/* W25Qxx Opcodes (Modular / Test suite naming) */
#define W25QXX_CMD_WRITE_ENABLE     0x06
#define W25QXX_CMD_WRITE_DISABLE    0x04
#define W25QXX_CMD_READ_STATUS_R1   0x05
#define W25QXX_CMD_READ_STATUS_R2   0x35
#define W25QXX_CMD_PAGE_PROGRAM     0x02
#define W25QXX_CMD_SECTOR_ERASE_4K  0x20
#define W25QXX_CMD_READ_DATA        0x03
#define W25QXX_CMD_JEDEC_ID         0x9F

#define W25QXX_STATUS_BUSY          0x01
#define W25QXX_STATUS_WEL           0x02

/* TV4 Canonical APIs from Google Doc */
int Flash_Init(void);
int Flash_SaveConfig(const System_Config_t *config);
int Flash_LoadConfig(System_Config_t *config);

/* W25Qxx Driver APIs */
bool w25qxx_init(void);
uint32_t w25qxx_read_jedec_id(void);
void w25qxx_set_mock_jedec_id(uint8_t mfg, uint8_t type, uint8_t cap);
bool w25qxx_erase_sector(uint32_t sector_addr);
bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len);
bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint16_t len);
bool w25qxx_save_config(const flash_config_block_t *cfg);
bool w25qxx_load_config(flash_config_block_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* W25QXX_H */
