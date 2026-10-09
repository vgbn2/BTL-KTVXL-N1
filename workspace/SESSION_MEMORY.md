# Session Memory & Durable Hardware Constraints

## Hardware Specifics (740k BOM)
- **STM32 Board:** STM32F103C8T6 "Blue Pill" (ARM Cortex-M3 @ 72 MHz, 64KB Flash, 20KB SRAM).
- **ESP32 Board:** ESP-WROOM-32 with CH340 USB-UART bridge (Dual Xtensa LX6 @ 240 MHz, 520KB SRAM).
- **IMU:** GY-521 Breakout (InvenSense MPU6050 6-Axis Accel + Gyro). I2C Address = `0x68` (AD0 tied to GND).
- **SPI Flash:** Winbond W25Q64FV (64M-Bit / 8MB). Operates strictly at 2.7V–3.6V (3.3V ONLY). `/WP` and `/HOLD` pulled HIGH to 3.3V.
- **Display:** 2.8" SPI TFT LCD (ILI9341 320x240 RGB color, Non-touch). Connected to ESP32 VSPI (`GPIO 15, 2, 4, 18, 23`).
- **Power:** MB-102 Breadboard Power Supply Module supplying 5V and 3.3V rails across 3x MB-102 Breadboards with common GND reference.

## Inter-Processor Protocol
- **Baudrate:** 115200 bps, 8N1 (STM32 `PA9`/`PA10` $\leftrightarrow$ ESP32 `GPIO 16`/`17`).
- **Commands:** `CALIBRATE_IMU\n`, `SET_FILTER_ALPHA:0.98\n`, `GET_ANGLE\n`.
- **Telemetry:** `ROLL:12.4\n`, `PITCH:-5.8\n`.
