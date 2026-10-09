# STM32F103C8T6 Architecture & Peripheral Allocation

## 1. System Clocks & Core
- **Core:** ARM 32-bit Cortex-M3 @ 72 MHz (HSE 8 MHz external crystal + PLL x9).
- **Flash / SRAM:** 64 KB Flash, 20 KB SRAM.
- **SysTick / Timer:** TIM2 / TIM3 configured to generate 100 Hz interrupt (10 ms period) for complementary filter step.

## 2. Peripheral Mapping
- **I2C1 (MPU6050):**
  - `PB6`: SCL (I2C Clock)
  - `PB7`: SDA (I2C Data)
- **SPI1 (W25Q64 Flash):**
  - `PA4`: NSS / CS (Software GPIO Output)
  - `PA5`: SCK (Master Clock)
  - `PA6`: MISO (Master In Slave Out)
  - `PA7`: MOSI (Master Out Slave In)
- **USART1 (Inter-processor Telemetry to ESP32):**
  - `PA9`: TX (Transmit)
  - `PA10`: RX (Receive)
  - Baudrate: 115200 bps, 8N1
- **SWD Debug Header:**
  - `PA13`: SWDIO
  - `PA14`: SWCLK
