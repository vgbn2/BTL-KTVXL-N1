# Winbond W25Qxx (W25Q64FV) SPI Flash Reference

## 1. Specifications & Hardware
- **Capacity:** 64M-bit / 8M-Byte (W25Q64) organized into 32,768 pages (256 bytes per page).
- **Interface:** SPI Mode 0 / Mode 3 (Clock up to 104 MHz).
- **Operating Voltage:** 2.7V to 3.6V (Direct 3.3V power).
- **Erase Sectors:** 4KB Sector, 32KB/64KB Block, Full Chip Erase.

## 2. SPI Command Instructions
| Command | Opcode (Hex) | Action |
| :--- | :--- | :--- |
| `W25Q_WRITE_ENABLE` | `0x06` | Set Write Enable Latch (WEL bit) |
| `W25Q_WRITE_DISABLE` | `0x04` | Reset WEL bit |
| `W25Q_READ_STATUS_1` | `0x05` | Read Status Register 1 (Check BUSY bit 0) |
| `W25Q_READ_DATA` | `0x03` | Read Data bytes (requires 24-bit address) |
| `W25Q_PAGE_PROGRAM` | `0x02` | Write up to 256 bytes (requires 24-bit address) |
| `W25Q_SECTOR_ERASE_4K` | `0x20` | Erase 4KB sector (requires 24-bit address) |
| `W25Q_JEDEC_ID` | `0x9F` | Read Manufacturer ID (`0xEF`) & Device ID (`0x4017`) |

## 3. Flash Storage Layout for BTL Project
- **Sector 0 (`0x000000` - `0x000FFF`):** System Calibration & Configuration:
  - Magic Word (`0xAA55BEEF`)
  - Filter Alpha (`float alpha`)
  - Accel Offsets (`int16_t offset_ax, offset_ay, offset_az`)
  - Gyro Offsets (`int16_t offset_gx, offset_gy, offset_gz`)
  - Checksum (CRC32)
