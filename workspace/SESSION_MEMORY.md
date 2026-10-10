# Session Memory & Durable Hardware Constraints

## Hardware Specifics (Physical BOM)
- **STM32 Board:** STM32F103C8T6 Blue Pill Type-C. Use board silkscreen labels in user-facing wiring docs: `A4`, `A5`, `A6`, `A7`, `B6`, `B7`, `A9`, `A10`, not only `PAx/PBx` formal MCU labels.
- **ESP32 Board:** ESP-WROOM-32 CH340 30-pin Type-C. UART2 pins are `GPIO 16` (RX2) and `GPIO 17` (TX2) on the right-side header; `VIN/5V` is bottom-left.
- **IMU:** GY-521 Breakout (InvenSense MPU6050 6-Axis Accel + Gyro). I2C Address = `0x68` (AD0 tied to GND). STM32 `B6` -> SCL, `B7` -> SDA.
- **SPI Flash:** W25Q64 6-pin breakout. Operates strictly at 2.7V–3.6V (3.3V ONLY). Pins: `VCC`, `CS`, `DO`, `GND`, `CLK`, `DI`; onboard SMD pull-ups handle `/WP` and `/HOLD`.
- **Display:** 2.8" SPI TFT LCD (ILI9341 320x240 RGB color, Non-touch). Connected to ESP32 VSPI (`GPIO 15`, `GPIO 2`, `GPIO 4`, `GPIO 18`, `GPIO 23`).
- **Power Rails:** It is safe to extend STM32 `3.3V` and `GND` to the breadboard vertical rails, then tap MPU6050/W25Q64 VCC/GND from those rails. Bridge rail splits if the breadboard rail is separated in the middle. Never connect 3.3V rail to 5V rail.

## Inter-Processor Protocol
- **Baudrate:** 115200 bps, 8N1 (STM32 `A9`/`A10` <-> ESP32 `GPIO 16`/`GPIO 17`).
- **Commands:** `CALIBRATE_IMU\n`, `SET_FILTER_ALPHA:0.98\n`, `GET_ANGLE\n`.
- **Telemetry:** `ROLL:12.4\n`, `PITCH:-5.8\n`.

## Next Session Focus
- Write physical pin tables using board silkscreen labels first, formal MCU labels second.
- Write hardware testing scripts/probes.
- Draft an academic PDF report with citations explaining why each pin layout was selected.
