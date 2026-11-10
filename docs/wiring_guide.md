# System Wiring & Pinout Guide (Centralized Single-MCU Architecture)


**Project:** Embedded Inclinometer with MPU6050 6-DOF IMU, W25Q64 SPI Flash, and ILI9341 2.8\" SPI TFT  
**Host Controller:** STM32F103C8T6 (ARM Cortex-M3 @ 72 MHz)  
**Layout:** Single Breadboard, Top-to-Bottom Component Stack  


---


## 1. Breadboard Wiring: Direct Point-to-Point Map


```text
========================================================================================
                      BREADBOARD WIRING: DIRECT POINT-TO-POINT MAP
========================================================================================


  [TOP: ILI9341 2.8\" SPI TFT LCD]
   (1) VCC         -----------------------------------------> 5V Power Rail
   (2) GND         -----------------------------------------> System GND Rail
   (3) CS          -----------------------------------------> STM32 PB12
   (4) RESET       -----------------------------------------> STM32 PB0
   (5) DC / RS     -----------------------------------------> STM32 PB1
   (6) SDI / MOSI  -----------------------------------------> STM32 PB15
   (7) SCK         -----------------------------------------> STM32 PB13
   (8) LED (BL)    -----------------------------------------> 3.3V Power Rail (Backlight ON)
   (9) SDO / MISO  [NC / Leave open - Display driver is write-only]


  --------------------------------------------------------------------------------------


  [UPPER-MID: GY-521 MPU-6050 6-DOF IMU]
   VCC             -----------------------------------------> 3.3V Power Rail
   GND             -----------------------------------------> System GND Rail
   SCL             -----------------------------------------> STM32 PB6 (I2C1_SCL)
   SDA             -----------------------------------------> STM32 PB7 (I2C1_SDA)
   AD0             -----------------------------------------> System GND Rail (Address 0x68), if wired to 3v3->high-> register address =0x69
   INT / XDA / XCL [NC / Leave open]


  --------------------------------------------------------------------------------------


  [MID: WINBOND W25Q64 8MB SPI NOR FLASH]
   (1) CS / NSS    -----------------------------------------> STM32 PA4 (SPI1_NSS)
   (2) DO / MISO   -----------------------------------------> STM32 PA6 (SPI1_MISO)
   (3) WP          [Pre-tied to 3.3V on breakout board]
   (4) GND         -----------------------------------------> System GND Rail
   (5) DI / MOSI   -----------------------------------------> STM32 PA7 (SPI1_MOSI)
   (6) CLK / SCK   -----------------------------------------> STM32 PA5 (SPI1_SCK)
   (7) HOLD        [Pre-tied to 3.3V on breakout board]
   (8) VCC         -----------------------------------------> 3.3V Power Rail (DO NOT CONNECT 5V)


  --------------------------------------------------------------------------------------


  [BOTTOM: STM32 BLUE PILL (LOOKING DOWN FROM USB TOP)]


       LEFT ROW (From USB Top Down)           RIGHT ROW (From USB Top Down)
      +----------------------------+         +----------------------------+
      | PB12 -> TFT CS             |         | GND  -> System GND Rail    |
      | PB13 -> TFT SCK            |         | GND  -> System GND Rail    |
      | PB14 -> [NC]               |         | 3V3  -> 3.3V Power Rail    |
      | PB15 -> TFT MOSI           |         | NRST -> [Reset Button]     |
      | PA8  -> [NC]               |         | PB11 -> [NC]               |
      | PA9  -> PC TXD (115200)    |         | PB10 -> [NC]               |
      | PA10 -> PC RXD (115200)    |         | PB1  -> TFT DC / RS        |
      | PA11 -> [NC]               |         | PB0  -> TFT RESET          |
      | PA12 -> [NC]               |         | PA7  -> Flash DI (MOSI)    |
      | PA13 -> ST-Link SWDIO      |         | PA6  -> Flash DO (MISO)    |
      | PA14 -> ST-Link SWCLK      |         | PA5  -> Flash SCK (CLK)    |
      | PA15 -> [NC]               |         | PA4  -> Flash CS (NSS)     |
      | PB3..5  [NC]               |         | PA0..3  [NC]               |
      | PB6  -> MPU6050 SCL        |         | PC14..15[NC]               |
      | PB7  -> MPU6050 SDA        |         | PC13 -> Green LED (Onboard)|
      | 5V   -> 5V Power Rail      |         | VBAT -> [NC]               |
      | GND  -> System GND Rail    |         | GND  -> System GND Rail    |
      | 3V3  -> 3.3V Power Rail    |         | 3V3  -> 3.3V Power Rail    |
      +----------------------------+         +----------------------------+


  --------------------------------------------------------------------------------------


  [BOTTOM HEADER: ST-LINK V2 SWD]
   Pin 1 (3.3V)    -----------------------------------------> ST-Link 3.3V (Red)
   Pin 2 (SWDIO)   -----------------------------------------> ST-Link SWDIO / PA13 (Yellow)
   Pin 3 (SWCLK)   -----------------------------------------> ST-Link SWCLK / PA14 (Green/Brown)
   Pin 4 (GND)     -----------------------------------------> ST-Link GND (Black/Blue)
```


---


## 2. Complete Wire Connection Checklist


Use this checklist to wire and verify all connections on the breadboard:


| # | From Peripheral | Pin | To STM32 / Power Rail | Recommended Color | Purpose |
|:---|:---|:---|:---|:---|:---|
| **1** | **TFT LCD** | VCC | **5V Power Rail** | Red | Power module (via onboard 3.3V LDO) |
| **2** | **TFT LCD** | GND | **System GND Rail** | Black | Ground |
| **3** | **TFT LCD** | CS | **STM32 PB12** | Purple | SPI2 Chip Select |
| **4** | **TFT LCD** | RESET | **STM32 PB0** | Brown | Hardware Reset |
| **5** | **TFT LCD** | DC | **STM32 PB1** | Orange | Data/Command Mode Select |
| **6** | **TFT LCD** | SDI / MOSI | **STM32 PB15** | Green | SPI2 Data (Master Out) |
| **7** | **TFT LCD** | SCK | **STM32 PB13** | Yellow | SPI2 Clock |
| **8** | **TFT LCD** | LED | **3.3V Power Rail** | Red | Backlight power (Always ON) |
| **9** | **TFT LCD** | SDO / MISO | *Leave Open (NC)* | — | Not used (Write-only driver) |
| **10**| **MPU-6050** | VCC | **3.3V Power Rail** | Red | Sensor power |
| **11**| **MPU-6050** | GND | **System GND Rail** | Black | Ground |
| **12**| **MPU-6050** | SCL | **STM32 PB6** | Green | I2C1 Clock (400 kHz) |
| **13**| **MPU-6050** | SDA | **STM32 PB7** | Brown | I2C1 Data |
| **14**| **MPU-6050** | AD0 | **System GND Rail** | Black | Sets I2C Address to `0x68` |
| **15**| **W25Q64 Flash**| VCC | **3.3V Power Rail** | Red | Flash power (**Never 5V**) |
| **16**| **W25Q64 Flash**| GND | **System GND Rail** | Black | Ground |
| **17**| **W25Q64 Flash**| CS | **STM32 PA4** | Orange | SPI1 Chip Select |
| **18**| **W25Q64 Flash**| CLK | **STM32 PA5** | Orange | SPI1 Clock (18 MHz) |
| **19**| **W25Q64 Flash**| DO / MISO | **STM32 PA6** | Orange | SPI1 Master In |
| **20**| **W25Q64 Flash**| DI / MOSI | **STM32 PA7** | Orange | SPI1 Master Out |
| **21**| **USB-TTL** | RXD | **STM32 PA9 (TX)** | Yellow | PC Telemetry Stream (115200) |
| **22**| **USB-TTL** | TXD | **STM32 PA10 (RX)**| White | PC CLI Commands (115200) |
| **23**| **USB-TTL** | GND | **System GND Rail** | Black | Common reference ground |


---


## 3. Bus Separation & Speed Configuration


| Peripheral Bus | Pins Used | Clock Speed | Dedicated Purpose |
|:---|:---|:---|:---|
| **SPI1** | PA4 (CS), PA5 (SCK), PA6 (MISO), PA7 (MOSI) | 18 MHz | Winbond W25Q64 NOR Flash (Calibration & Logs) |
| **SPI2** | PB12 (CS), PB13 (SCK), PB15 (MOSI), PB0 (RST), PB1 (DC) | 18 MHz | ILI9341 2.8\" Color TFT Display (Live Visuals) |
| **I2C1** | PB6 (SCL), PB7 (SDA) | 400 kHz | MPU-6050 IMU Data Sampling (100 Hz Loop) |
| **USART1** | PA9 (TX), PA10 (RX) | 115200 bps | Telemetry stream & ASCII CLI parser |
| **SWD** | PA13 (SWDIO), PA14 (SWCLK) | 4 MHz | ST-Link V2 In-Circuit Flash & Debug |