# Dual-MCU STM32 + ESP32 Architecture & Hardware Partitioning

**Document Version:** 1.0  
**Target Systems:** STM32F103C8T6 (Sensor/Real-Time Node) + ESP32-WROOM-32 (HMI/Connectivity Gateway)

---

## 1. System Motivation: Why Dual-MCU?

The tilt measurement and motion tracking system adopts a dual-microcontroller architecture to guarantee strict separation of concerns between **Hard Real-Time Sensor Processing** and **High-Throughput Graphic/Network Presentation**.

```text
+-------------------------------------------------------------------------+
|                              SYSTEM TOPOLOGY                            |
+-------------------------------------------------------------------------+
|                                                                         |
|  +--------------------+        I2C (400kHz)      +-------------------+  |
|  |   GY-521 MPU6050   | <----------------------> |                   |  |
|  +--------------------+                          |   STM32F103C8T6   |  |
|                                                  |   (Cortex-M3)     |  |
|  +--------------------+        SPI (18MHz)       |                   |  |
|  |   W25Q64 Flash     | <----------------------> |   72 MHz SYSCLK   |  |
|  +--------------------+                          +---------+---------+  |
|                                                            |            |
|                                                     UART   | 115200 8N1 |
|                                                     Full   | 15-Byte    |
|                                                     Duplex | Frame      |
|                                                            v            |
|  +--------------------+        SPI (27MHz)       +---------+---------+  |
|  |  ILI9341 2.8" TFT  | <----------------------- |      ESP32        |  |
|  +--------------------+                          |   (Dual Xtensa)   |  |
|                                                  |                   |  |
|  +--------------------+        Bluetooth BLE     |   240 MHz SYSCLK  |  |
|  | Smartphone / Web   | <~~~~~~~~~~~~~~~~~~~~~~~ |                   |  |
|  +--------------------+                          +-------------------+  |
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Single-MCU Bottleneck Analysis

When an engineer attempts to implement both high-speed IMU motion estimation and high-resolution display rendering on a single microcontroller, three fatal bottlenecks emerge:

1. **Deterministic Timer Jitter vs. Display Latency:**
   - Numerical integration of angular velocity ($\theta = \int \omega \, dt$) requires strict, equidistant sampling ($\Delta t = 10\text{ ms} \pm 10\ \mu\text{s}$ at $100\text{ Hz}$).
   - Pushing a frame of graphics to an ILI9341 display over SPI takes between $20\text{ ms}$ and $45\text{ ms}$. If the CPU is locked in display transmission loops, timer interrupts are either preempted or delayed, causing mathematical drift in the complementary filter.
2. **RTOS Scheduling Jitter in WiFi/BLE Stacks:**
   - On the ESP32, the WiFi and Bluetooth Low Energy (BLE) radio stacks run as high-priority FreeRTOS tasks. RF packet transmission causes core preemption lasting up to several milliseconds, corrupting I2C burst reads if the IMU is driven on the same chip.
3. **Hardware SPI Bus Contention:**
   - Both the W25Q64 Flash memory and the ILI9341 TFT display use SPI. Sharing a single SPI peripheral requires multiplexing Chip Select pins and switching clock speeds (TFT operates at $\sim 27\text{ MHz}$, whereas Flash may require different modes), introducing bus contention and arbitration overhead.

---

## 2. Functional Task Partitioning Matrix

| Responsibility Area | Assigned MCU | Dedicated Hardware Peripheral | Execution Rationale |
|---|---|---|---|
| **MPU6050 I2C Sampling** | **STM32F103** | `I2C1` (`PB6`/`PB7`) @ $400\text{ kHz}$ | Hard real-time 100 Hz hardware timer trigger (`TIM2`). Zero RTOS latency. |
| **Motion Filter Math** | **STM32F103** | ARM Cortex-M3 Core @ $72\text{ MHz}$ | Computes $atan2()$, gyro integration, and complementary filter every $10\text{ ms}$. |
| **Flash Data Logging** | **STM32F103** | `SPI1` (`PA4`-`PA7`) @ $18\text{ MHz}$ | Writes calibration offsets and circular telemetry history without display bus interference. |
| **Telemetry Packaging**| **STM32F103** | `USART1` (`PA9`/`PA10`) @ $115200\text{ bps}$ | Serializes pitch, roll, yaw, and diagnostic flags into 15-byte frames with CRC16. |
| **Dashboard UI Render**| **ESP32** | Hardware `VSPI` (`GPIO18`, `23`, `15`, `2`) | Drives ILI9341 color display with rolling graphs, status meters, and dirty rectangle caching. |
| **Wireless Telemetry** | **ESP32** | Integrated 2.4 GHz Radio | Transmits tilt data via Bluetooth Low Energy (BLE GATT) and HTTP/WebSocket to phones/PCs. |
| **User Controls** | **ESP32** | GPIO Interrupts / Capacitive Touch | Handles mode buttons and tare/zeroing calibration triggers without loading the sensor loop. |

---

## 3. Physical Electrical Boundary & Signal Integrity

### 3.1 Voltage Domain Matching
- **STM32F103C8T6:** Operates at $V_{DD} = 3.3\text{V}$. Its USART1 pins (`PA9`, `PA10`) output CMOS levels where High $\approx 3.3\text{V}$ and Low $\approx 0\text{V}$.
- **ESP32-WROOM-32:** Operates at $V_{DD} = 3.3\text{V}$. Its GPIO pins expect $3.3\text{V}$ maximum logic levels (ESP32 pins are **not** 5V tolerant).
- **Conclusion:** Both chips operate natively at $3.3\text{V}$ logic levels. **No logic level converter or resistor divider is required on the UART lines.**

### 3.2 Inter-Board Wiring
```text
  STM32 Blue Pill                   ESP32 DevKit
+------------------+             +------------------+
| PA9  (USART1_TX) | ----------> | GPIO16 (RX2)     |
| PA10 (USART1_RX) | <---------- | GPIO17 (TX2)     |
| GND              | =========== | GND (COMMON)     |
+------------------+             +------------------+
```

### 3.3 Common Ground Rule (Critical)
A floating ground between the STM32 and ESP32 causes signal reference voltage drift, resulting in framing errors (`FE`), noise, and CRC checksum failures. A thick, direct ground wire must connect the `GND` pins of both development boards.

---

## 4. Clock Domains & Asynchronous Decoupling

The two microcontrollers operate in completely independent, unsynchronized clock domains:
- **STM32:** $8\text{ MHz}$ external quartz crystal $\rightarrow$ PLL $\times 9 = 72.000\text{ MHz}$ core clock.
- **ESP32:** $40\text{ MHz}$ crystal $\rightarrow$ PLL $= 240.000\text{ MHz}$ dual-core clock.

### Decoupling Mechanism: Asynchronous UART FIFO
- The UART protocol uses start/stop bit edge synchronization to absorb minor oscillator drift (within $\pm 2\%$).
- On the ESP32 side, the hardware UART2 peripheral features a built-in **128-byte hardware FIFO buffer**. Telemetry frames arriving from the STM32 are automatically buffered in silicon hardware, allowing the ESP32 CPU to read packets in batches between display draw calls without dropping incoming bytes.

---

## 5. Fault Isolation & Independent Watchdogs

The dual-MCU architecture establishes a strict **hardware fault barrier**:

1. **Independent Watchdog (IWDG) on STM32:**
   - If an I2C bus lockup occurs on the MPU6050, the STM32 hardware watchdog triggers an independent hardware reset in $100\text{ ms}$ without resetting the ESP32 display.
2. **Task Watchdog Timer (TWDT) on ESP32:**
   - If the ESP32 display loop or WiFi stack experiences a deadlock, the ESP32 resets independently without interrupting the STM32's sensor integration or flash logging.
3. **Graceful Disconnect Handling:**
   - If the UART connection between the boards is interrupted, the ESP32 displays an amber `[SENSOR OFFLINE]` indicator on the dashboard while retaining the last valid reading, recovering automatically upon packet reception.
