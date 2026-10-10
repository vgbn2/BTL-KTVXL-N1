# Workspace State

**Project:** BTL-KTVXL-N1 (Thiết bị đo góc nghiêng sử dụng MPU6050, W25Qxx, STM32F103 Type-C, ESP32 Type-C, ILI9341 2.8" SPI TFT)  
**Branch:** `feature/hardware-wiring-and-docs`  
**Status:** Firmware implementation, unit tests, predevelopment package, simulation configurations, and hardware documentation complete and verified. Ready for physical hardware testing and board bring-up.

## Completed Artifacts & Codebase
- **Predevelopment Package:** `docs/Project_Predevelopment_Package.md` (967 lines, complete 16-section standard with C4 diagrams, ADRs, state machines, and bidirectional traceability matrix).
- **Core Firmware Modules:**
  - `src/filter.c` & `include/filter.h`: 100 Hz Complementary filter ($\alpha = 0.98, \Delta t = 0.01\text{s}$) with offset calibration.
  - `src/protocol.c` & `include/protocol.h`: 15-byte binary telemetry frame codec with CRC16-CCITT + ASCII command parser.
  - `src/storage_format.c` & `include/storage_format.h`: W25Q64 Sector 0 config block serialization with CRC32 integrity check.
  - `src/mpu6050.c` & `include/mpu6050.h`: I2C1 burst read (14 bytes) & stationary calibration.
  - `src/w25qxx.c` & `include/w25qxx.h`: SPI1 Nor-flash sector erase, page write, and config load/save.
  - `src/main.c`: STM32 main application with TIM2 100 Hz deterministic loop and USART1 streaming.
- **ESP32 Display & Telemetry:**
  - `src/esp32_firmware/esp32_firmware.ino`: 320x240 ILI9341 landscape dashboard with digital readouts, horizon crosshair, and UART2 bridge.
  - `src/esp32_standalone_sim/esp32_standalone_sim.ino`: Standalone sketch for browser 3D MPU6050 tilt simulation.
- **Automated Host Test Suite:**
  - `tests/test_firmware_core.c` & `Makefile`: 100% passing tests for filter math, protocol codec, ASCII commands, and flash config CRC32.
- **Simulation Suite (Wokwi):**
  - `diagram.json`, `wokwi.toml`, `libraries.txt`, `wokwi_standalone.json`: Complete simulation netlists.
- **Hardware Documentation:**
  - `docs/Wiring_and_Pinouts.md`: Complete pinout tables, ST-Link SWD, and Section 2.5 ILI9341 rectangle pinout diagram.
  - `docs/interactive-wiring.html` & `scripts/verify_hardware_sim.js`: Interactive visual studio and automated SVG pin integrity checker (22 nets, 85 pin sockets verified).
