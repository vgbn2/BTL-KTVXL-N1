# ESP32-WROOM-32 (30-Pin DevKit) Microcontroller Deep Dive

**Core:** Dual-Core 32-bit Xtensa LX6 @ 240 MHz (Tensilica)  
**Connectivity:** Integrated 2.4 GHz Wi-Fi (802.11 b/g/n) + Bluetooth v4.2 BR/EDR & BLE  
**Role:** HMI Display Driver (ILI9341 320x240 TFT), BLE Gateway & Telemetry Dashboard

---

## 1. Physical Pin Layout & Hardware Pinout (30-Pin DevKit V1)

```text
                                 [ Micro-USB / Type-C ]
                                   +-----------------+
                           EN/RST -| [1]        [30] |- GPIO23 (VSPI_MOSI)
                           GPIO36 -| [2]        [29] |- GPIO22 (I2C_SCL)
                           GPIO39 -| [3]        [28] |- GPIO1  (TX0 / Console)
                           GPIO34 -| [4]        [27] |- GPIO3  (RX0 / Console)
                           GPIO35 -| [5]        [26] |- GPIO21 (I2C_SDA)
                           GPIO32 -| [6]        [25] |- GND
                           GPIO33 -| [7]        [24] |- GPIO19 (VSPI_MISO)
                           GPIO25 -| [8]        [23] |- GPIO18 (VSPI_SCK)
                           GPIO26 -| [9]        [22] |- GPIO5  (TFT_BL / CS)
                           GPIO27 -| [10]       [21] |- GPIO17 (TX2 -> STM32)
                           GPIO14 -| [11]       [20] |- GPIO16 (RX2 <- STM32)
                           GPIO12 -| [12]       [19] |- GPIO4  (TFT_RST)
                           GND    -| [13]       [18] |- GPIO0  (BOOT Button)
                           GPIO13 -| [14]       [17] |- GPIO2  (TFT_DC)
                           GPIO9  -| [15]       [16] |- GPIO15 (TFT_CS)
                                   +-----------------+
```

### 1.1 Pin Function & Project Assignments

| ESP32 Pin | Type | Dedicated Peripheral | Connected Hardware | Hardware Rules & Notes |
|---|---|---|---|---|
| **`GPIO16`** | Input | `UART2_RX` | STM32 `PA9` (`USART1_TX`) | Inter-MCU Telemetry RX. 128-byte hardware FIFO. |
| **`GPIO17`** | Output | `UART2_TX` | STM32 `PA10` (`USART1_RX`)| Inter-MCU Command TX. |
| **`GPIO18`** | Output | `VSPI_SCK` | TFT `SCK / CLK` | SPI Master Clock ($27\text{ MHz} - 40\text{ MHz}$). |
| **`GPIO23`** | Output | `VSPI_MOSI` | TFT `SDI / MOSI` | SPI Master Out Data stream. |
| **`GPIO19`** | Input | `VSPI_MISO` | TFT `SDO / MISO` (Optional) | SPI Master In (can be left floating for display). |
| **`GPIO15`** | Output | General GPIO | TFT `CS` | Chip Select (Active LOW). |
| **`GPIO2`** | Output | General GPIO | TFT `DC / RS` | Data/Command select ($0$=Cmd, $1$=Data). Strapping pin (must be LOW at boot). |
| **`GPIO4`** | Output | General GPIO | TFT `RESET` | Hardware Reset pulse. |
| **`GPIO5`** | Output | `LEDC_PWM` / GPIO| TFT `LED / BLK` | Backlight drive / PWM dimming. Strapping pin (outputs PWM). |
| **`GPIO32`** | Output | General GPIO | TFT `LED` Compatibility | Secondary backlight compatibility probe. |
| **`GPIO0`** | In/Out | Strapping Pin | Onboard BOOT Button | Pulled HIGH internally. Pull LOW during boot to enter UART Download mode. |
| **`3.3V`** | Power | Out ($3.3\text{V}$) | Sensor rails | Output from onboard AMS1117/ME6211 3.3V regulator ($800\text{ mA}$ max). |
| **`VIN / 5V`**| Power | In/Out ($5.0\text{V}$) | TFT VCC Power | Directly connected to USB 5V rail. Powers TFT backlight safely. |

### 1.2 Critical Hardware Strapping Pins (Avoid Conflicts)
The ESP32 samples five strapping pins during power-up/reset to configure internal flash voltage and boot mode:
- **`GPIO 0`:** Must be **HIGH** for normal SPI Flash boot. Grounding it enters flashing mode.
- **`GPIO 2`:** Connected to onboard blue LED. Must be **LOW or Floating** during flashing.
- **`GPIO 12` (MTDI):** Controls flash voltage ($3.3\text{V}$ vs $1.8\text{V}$). **Never pull HIGH externally**, or flash memory will be under-volted and brick the bootloader.
- **`GPIO 34 - 39`:** **Input-Only Pins.** They lack internal software pull-up and pull-down resistors and cannot be configured as outputs.

---

## 2. Datasheet Specifications & Architecture

| Parameter | Specification | Engineering Impact |
|---|---|---|
| **CPU Architecture** | Tensilica Xtensa Dual-Core 32-bit LX6 | Dual cores (`PRO_CPU` and `APP_CPU`) running up to $240\text{ MHz}$ ($600\text{ DMIPS}$). |
| **Internal SRAM** | $520\text{ KB}$ ($320\text{ KB}$ user heap) | Ample RAM for graphics buffers, BLE packets, and dirty rectangle caching. |
| **External Flash** | $4\text{ MB}$ SPI Flash (Winbond/GigaDevice) | Holds ESP32 firmware, SPIFFS/LittleFS storage, and font tables. |
| **Operating Voltage** | $3.0\text{V} - 3.6\text{V}$ (Nominal $3.3\text{V}$) | Logic levels are strictly 3.3V CMOS. **Pins are NOT 5V tolerant.** |
| **Active RF Current** | $160\text{ mA} - 240\text{ mA}$ peak during Wi-Fi/BT bursts | Power supply must provide at least $500\text{ mA}$ clean current to avoid brownout resets. |

---

## 3. Peripheral Hardware & FIFO Memory Architecture

### 3.1 Hardware UART Controllers (`UART0`, `UART1`, `UART2`)
The ESP32 features three independent full-duplex UART peripherals. 
- **UART0:** Dedicated to flashing and serial debug console (`GPIO1`/`GPIO3`).
- **UART2:** Fully assignable via the GPIO Matrix to any external pins. In our architecture, `UART2` is routed to `GPIO16` (RX) and `GPIO17` (TX).
- **128-Byte Hardware FIFO:** Incoming telemetry bytes from the STM32 are ingested into the hardware RX FIFO in silicon without CPU intervention. Even if the ESP32 is busy drawing graphics for $30\text{ ms}$, up to 8 full 15-byte telemetry frames are safely preserved in the FIFO without packet loss.

### 3.2 Hardware SPI Controllers (`SPI0`, `SPI1`, `HSPI`, `VSPI`)
- `SPI0` & `SPI1` are reserved internally for the 4MB SPI flash cache.
- `VSPI` (SPI3) is the primary high-speed bus used for the ILI9341 display. It features dedicated hardware DMA (Direct Memory Access) channels capable of streaming 64-byte chunks directly from RAM to the display at up to $40\text{ MHz}$ clock rates.

---

## 4. Software Interaction Layers & Libraries

### 4.1 Dual-Core FreeRTOS Task Distribution
Under Arduino-ESP32 / ESP-IDF, FreeRTOS distributes workload across both CPU cores:
- **Core 0 (`PRO_CPU`):** Runs background networking stacks (WiFi driver, Bluetooth LE controller, TCP/IP stack).
- **Core 1 (`APP_CPU`):** Executes the Arduino `setup()` and `loop()`, driving the ILI9341 UI rendering and processing UART2 packets.

### 4.2 Hardware UART Initialization (Arduino C++)
```cpp
/* Initialize Hardware Serial 2 on pins 16 (RX) and 17 (TX) */
void setupUART() {
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    Serial2.setRxBufferSize(256); // Allocate 256-byte software ring buffer
}

/* Non-blocking frame ingestion */
void readTelemetry() {
    while (Serial2.available() >= 15) {
        if (Serial2.read() == 0xAA && Serial2.peek() == 0x55) {
            Serial2.read(); // Consume 0x55
            // Read remaining 13 bytes into frame buffer...
        }
    }
}
```

### 4.3 Display Rendering Interaction (Adafruit_ILI9341 vs TFT_eSPI)
```cpp
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

void initDisplay() {
    tft.begin(27000000); // Drive SPI at 27 MHz
    tft.setRotation(1);   // Landscape mode (320x240)
    tft.fillScreen(ILI9341_BLACK);
}
```
