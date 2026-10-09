# Next Session Goal

1. **Review Hardware & Wiring Implementation:**
   - Verify wiring on physical breadboard based on `docs/interactive-wiring.html` and `docs/Wiring_and_Pinouts.md`.
2. **Datasheet-Driven Firmware Driver Implementation:**
   - **STM32 MPU6050 Driver (`lib/mpu6050`):** I2C1 initialization, wake-up (`PWR_MGMT_1`), DLPF config, raw accelerometer and gyroscope reading, static bias calibration.
   - **STM32 Complementary Filter Engine:** Hardware Timer (`TIM2`/`TIM3`) 100 Hz ISR for deterministic $\Delta t = 10\text{ ms}$ angle integration.
   - **STM32 W25Qxx SPI Flash Driver (`lib/w25qxx`):** SPI1 hardware driver, sector erase (`0x20`), page program (`0x02`), and calibration parameter storage struct persistence.
   - **STM32 UART Telemetry & Command Parser (`lib/uart`):** Handle `CALIBRATE_IMU`, `SET_FILTER_ALPHA`, `GET_ANGLE`, outputting `ROLL:xx.x`, `PITCH:yy.y`.
   - **ESP32 Display & Dashboard Firmware:** Receive UART packets from STM32 and render real-time Roll/Pitch gauges on ILI9341 2.8" SPI TFT screen.
