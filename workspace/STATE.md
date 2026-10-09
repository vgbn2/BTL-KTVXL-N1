# Workspace State

**Project:** BTL-KTVXL-N1 (Thiết bị đo góc nghiêng sử dụng MPU6050, W25Qxx, STM32F103, ESP32, ILI9341 2.8" SPI TFT)  
**Branch:** `main`  
**Status:** Hardware wiring architecture, pinout maps, interactive drag/rotate simulator, and hardware rationale fully documented. Ready for firmware driver implementation.

## Completed Artifacts & Documentation
- `docs/interactive-wiring.html`: Standalone interactive circuit canvas with draggable modules, 90-degree rotation, pan/zoom, dynamic Bézier wire recalculation, bus filters (I2C, SPI1, VSPI, UART, Power), and layout presets matching the 740k component kit (3x MB-102 Breadboards, MB-102 Power Supply, STM32 Blue Pill, ESP32 CH340, GY-521 MPU6050, W25Q64 Flash, 2.8" SPI TFT).
- `docs/Wiring_and_Pinouts.md`: Full ASCII pinout diagrams, exact pin-by-pin tables, and UART command protocol definitions.
- `docs/Hardware_Architecture_and_Wiring_Rationale.md`: Engineering analysis of dual-processor partitioning (STM32 hard real-time 100Hz filter loop vs ESP32 graphics host), bus signal integrity, FMEA failure analysis, and power budgeting.
- `docs/Research/MPU6050.md`: Sensor register map, DLPF configuration, and Complementary filter formulas.
- `docs/Research/FlashW250Qxx.md`: Winbond W25Q64 SPI opcodes, sector layout, and calibration storage schema.
- `docs/Research/STM32.md`: STM32F103 peripheral allocation (I2C1 PB6/PB7, SPI1 PA4..PA7, USART1 PA9/PA10).
