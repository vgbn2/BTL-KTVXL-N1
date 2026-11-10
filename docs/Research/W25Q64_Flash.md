# Winbond W25Q64FV Serial Flash Memory Deep Dive

**Datasheet Reference:** Winbond Electronics Corp. *W25Q64FV: 3V 64M-Bit Serial Flash Memory with Dual/Quad SPI and QPI*, Publication Release Date: June 03, 2013, Revision J.  
**Target Hardware:** Winbond W25Q64FV (SOIC-8 208-mil) on Breakout Board  
**Target Bus:** STM32F103 `SPI1` Master (`PA4` CS, `PA5` SCK, `PA6` MISO, `PA7` MOSI) @ $18\text{ MHz}$

---

## 1. Physical Pin Layout & Package Configuration (Datasheet Section 3.1, p. 7)

### 8-Pin SOIC 208-mil Pinout

```text
               +-----------------------+
               |   W25Q64FV (SOIC-8)   |
               |                       |
 [ 1 ] /CS     | /CS               VCC | [ 8 ] ---> 3.3V Power (2.7V - 3.6V)
 [ 2 ] DO      | DO (IO1)   /HOLD(IO3) | [ 7 ] ---> 3.3V (Hold inactive)
 [ 3 ] /WP     | /WP (IO2)         CLK | [ 6 ] ---> STM32 PA5 (SPI1_SCK)
 [ 4 ] GND     | GND          DI (IO0) | [ 5 ] ---> STM32 PA7 (SPI1_MOSI)
               +-----------------------+
```

### Pin Description Table (Datasheet Section 3.2, p. 8)

| Pin | Name | I/O | Logic Level | Function Description |
|---|---|---|---|---|
| **`1`** | **`/CS`** | Input | $3.3\text{V}$ (Active LOW) | **Chip Select:** Drives device selection and initiates every SPI transaction. When /CS is HIGH, power consumption drops to standby ($1\ \mu\text{A}$ typical). |
| **`2`** | **`DO`** | Output| $3.3\text{V}$ (CMOS) | **Data Output (MISO):** High-impedance when /CS is HIGH. Transmits data on falling edge of CLK. |
| **`3`** | **`/WP`** | Input | $3.3\text{V}$ (Active LOW) | **Write Protect:** Hardware protection for Status Registers. Tied permanently to $3.3\text{V}$ on module. |
| **`4`** | **`GND`** | Power | $0\text{V}$ | Common reference ground return. |
| **`5`** | **`DI`** | Input | $3.3\text{V}$ | **Data Input (MOSI):** Latches instruction opcodes, addresses, and data bytes on rising edge of CLK. |
| **`6`** | **`CLK`** | Input | $3.3\text{V}$ | **Serial Clock:** Supports SPI Mode 0 ($CPOL=0, CPHA=0$) and Mode 3 ($CPOL=1, CPHA=1$). |
| **`7`** | **`/HOLD`**| Input | $3.3\text{V}$ (Active LOW) | **Hold:** Pauses SPI transfer without deselecting chip. Tied permanently to $3.3\text{V}$ on module. |
| **`8`** | **`VCC`** | Power | $2.7\text{V} - 3.6\text{V}$ | Supply voltage. Peak active programming current: $25\text{ mA}$ max (Table 8.4, p. 73). |

---

## 2. Datasheet Memory Topology & Architectural Hierarchy

The memory array consists of $67,108,864\text{ bits}$ ($8,388,608\text{ bytes} = 8\text{ MB}$), structured as:

$$\text{8 MB Total} = 128\text{ Blocks (64 KB)} = 2,048\text{ Sectors (4 KB)} = 32,768\text{ Pages (256 Bytes)}$$

```text
[ 8 MB Memory Space: 0x000000 to 0x7FFFFF ]
 |
 +---> 128 Blocks (64 KB: Block 0 = 0x000000..0x00FFFF, Block 127 = 0x7F0000..0x7FFFFF)
        |
        +---> 16 Sectors per Block (4 KB: Sector 0 = 0x000000..0x000FFF, ...)
               |
               +---> 16 Pages per Sector (256 Bytes: Page 0 = 0x000000..0x0000FF, ...)
```

---

## 3. AC/DC Timing Specifications (Datasheet Table 8.6, p. 74)

| Parameter | Symbol | Min | Typical | Max | Unit | Engineering Consequence |
|---|---|---|---|---|---|---|
| **Read Data Clock Freq** | $f_R$ | DC | — | **`50`** | **MHz** | Fast enough for standard SPI reads at full STM32 speed ($18\text{ MHz}$). |
| **Fast Read Clock Freq** | $f_C$ | DC | — | **`104`**| **MHz** | Standard SPI / Dual SPI operation. |
| **Page Program Time** | $t_{\text{PP}}$ | — | **`0.7`** | **`3.0`** | **ms** | Programs up to 256 bytes. Software must poll `BUSY` bit. |
| **Sector Erase Time (4KB)**| $t_{\text{SE}}$ | — | **`30`** | **`400`** | **ms** | Mandatory before writing to dirty flash memory. |
| **Block Erase Time (32KB)**| $t_{\text{BE1}}$| — | **`120`**| **`800`** | **ms** | 32 KB block erase. |
| **Block Erase Time (64KB)**| $t_{\text{BE2}}$| — | **`150`**| **`1,000`**| **ms** | 64 KB block erase. |
| **Chip Erase Time** | $t_{\text{CE}}$ | — | **`20`** | **`100`** | **s** | Full 8 MB bulk erase. |
| **Write Enable to /CS High**| $t_{\text{SHSL1}}$| **`50`**| — | — | **ns** | /CS deselect time between consecutive SPI commands. |

---

## 4. Flash Physics: The Two Fundamental Invariant Rules

1. **Erase State is `0xFF`:**
   Flash cells store charge in floating gates. Erasing drains electrons, resetting all bits to binary `1` (`0xFF`).
2. **Bits can only be programmed from `1` to `0`:**
   Writing data injects electrons to turn `1` into `0`. An already programmed `0` cannot be turned back to `1` by writing; **the entire 4KB sector must be erased back to `0xFF`**.
3. **The 256-Byte Page Boundary Wrap Rule (Datasheet Section 7.2.14, p. 38):**
   A Page Program instruction (`0x02`) programs between $1$ and $256$ bytes. However:
   > *"If an entire 256 byte page is to be programmed, the last address byte (the 8 least significant address bits) should be set to 0. If the 8 least significant address bits (A7-A0) are not all zero, and the number of bytes to be programmed exceeds the remaining page length, the remaining bytes will be programmed from the beginning of the same page (A7-A0 = 0), and will overwrite previous data."*

---

## 5. Standard SPI Instruction Set (Datasheet Table 7.2, pp. 24–26)

| Instruction | Opcode | Addr Bytes | Dummy Bytes | Data Bytes | Function Description |
|---|---|---|---|---|---|
| **`Write Enable`** | `0x06` | 0 | 0 | 0 | Sets WEL bit in Status Register 1. Mandatory before any Write or Erase. |
| **`Write Disable`** | `0x04` | 0 | 0 | 0 | Clears WEL bit to protect against accidental writes. |
| **`Read Status Reg-1`**| `0x05` | 0 | 0 | $\ge 1$ | Reads Status Register 1. Continuously polled to monitor **`BUSY`** bit 0. |
| **`Read Status Reg-2`**| `0x35` | 0 | 0 | $\ge 1$ | Reads Status Register 2 (Quad Enable bit `QE`). |
| **`Read Data`** | `0x03` | 3 (24-bit) | 0 | $\ge 1$ | Sequentially streams data bytes. Address auto-increments. |
| **`Fast Read`** | `0x0B` | 3 (24-bit) | 1 | $\ge 1$ | High-speed read up to 104 MHz. Requires 1 dummy byte. |
| **`Page Program`** | `0x02` | 3 (24-bit) | 0 | $1 - 256$ | Writes up to 256 bytes into a single designated page. |
| **`Sector Erase (4KB)`**| `0x20` | 3 (24-bit) | 0 | 0 | Erases the designated 4KB sector back to all `0xFF`. |
| **`Block Erase (64KB)`**| `0xD8` | 3 (24-bit) | 0 | 0 | Erases the designated 64KB block back to all `0xFF`. |
| **`Chip Erase`** | `0xC7` / `0x60` | 0 | 0 | 0 | Erases the entire 8MB flash memory array. |
| **`Read JEDEC ID`** | `0x9F` | 0 | 0 | 3 | Reads Manufacturer ID (`0xEF`), Memory Type (`0x40`), Capacity (`0x17`). |
| **`Power-down`** | `0xB9` | 0 | 0 | 0 | Puts chip in deep sleep mode ($1\ \mu\text{A}$ current). |
| **`Release Power-down`**| `0xAB` | 0 | 3 | 1 | Wakes chip and returns Device ID (`0x16`). |

---

## 6. Status Register 1 Architecture (Datasheet Section 7.1.1, p. 18)

```text
  Bit 7      Bit 6      Bit 5      Bit 4      Bit 3      Bit 2      Bit 1      Bit 0
+----------+----------+----------+----------+----------+----------+----------+----------+
|   SRP0   |   SEC    |    TB    |   BP2    |   BP1    |   BP0    |   WEL    |   BUSY   |
+----------+----------+----------+----------+----------+----------+----------+----------+
```

- **Bit 0 (`BUSY`):**
  - Read-only bit set to `1` when the device is executing Page Program, Sector Erase, Block Erase, or Chip Erase.
  - Cleared to `0` when the internal write cycle completes.
- **Bit 1 (`WEL` - Write Enable Latch):**
  - Set to `1` after executing `0x06` (Write Enable).
  - Cleared to `0` automatically upon completion of any write or erase command.
- **Bits 4:2 (`BP2`, `BP1`, `BP0` - Block Protect):**
  - Non-volatile bits to write-protect sectors. For full read/write, set all three to `0`.

---

## 7. Memory Allocation for Tilt Sensor Logging

The $8\text{ MB}$ Flash memory is partitioned into two distinct logical zones:

### Zone 1: System Calibration & Configuration ($4\text{ KB}$ Sector 0)
- **Address Range:** `0x000000` to `0x000FFF`
- Holds sensor zero-motion offsets, filter parameters, and system identification:
  ```c
  struct SystemConfig {
      uint32_t magic_word;      // 0xAA55BEEF
      float filter_alpha;       // Default 0.98f
      int16_t accel_offset_x;   // Zero-motion X accel offset
      int16_t accel_offset_y;   // Zero-motion Y accel offset
      int16_t accel_offset_z;   // Zero-motion Z accel offset (1g reference)
      int16_t gyro_offset_x;    // Stationary X gyro bias
      int16_t gyro_offset_y;    // Stationary Y gyro bias
      int16_t gyro_offset_z;    // Stationary Z gyro bias
      uint32_t crc32;           // Structural integrity checksum
  };
  ```

### Zone 2: Circular Telemetry Log Buffer (Sectors 1 to 2047)
- **Address Range:** `0x001000` to `0x7FFFFF` ($8,384,512\text{ bytes}$)
- Organized as a ring buffer of 16-byte timestamped telemetry records:
  $$\text{Capacity} = \frac{8,384,512\text{ bytes}}{16\text{ bytes/record}} = 524,032\text{ historical records}$$
- At a 1 Hz periodic logging interval, the flash chip stores over **145 continuous hours** of pitch, roll, and alarm history before wrapping.
