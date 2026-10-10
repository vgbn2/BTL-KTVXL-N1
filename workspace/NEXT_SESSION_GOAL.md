# Next Session Goal

1. **Physical Hardware Wiring & Board Bring-Up:**
   - Plug breadboard jumpers per `docs/Wiring_and_Pinouts.md` (STM32 I2C1 to MPU6050, SPI1 to W25Q64, USART1 to ESP32 UART2, and VSPI to ILI9341).
   - Ensure common GND rail and strict 3.3V power to W25Q64 flash module.

2. **Firmware Flashing & Bench Verification:**
   - Flash STM32 using ST-Link V2 (`SWCLK`, `SWDIO`, `3.3V`, `GND`).
   - Flash ESP32 via USB Type-C using `arduino-cli compile --upload -p /dev/ttyUSB0 src/esp32_firmware`.
   - Send `CALIBRATE_IMU` over serial monitor and verify that zero-bias calibration saves to W25Q64 Sector 0.

3. **Live Testing & Video/Demo Documentation:**
   - Record pitch and roll responses on ILI9341 display across $+/-15^\circ$, $+/-45^\circ$, and flat orientations.
