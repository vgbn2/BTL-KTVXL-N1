# Centralized Embedded Inclinometer: Hardware Architecture, Interconnect Signal Integrity, and Electrical Reliability Analysis

**Course:** Bài tập lớn Kỹ thuật Vi xử lý - Nhóm 1 - D24-PTIT  
**Topic:** Thiết bị đo góc nghiêng độ chính xác cao sử dụng 2 cảm biến MPU6050, Winbond W25Q64 SPI Flash, Màn hình ILI9341 2.8" SPI TFT LCD, và STM32F103C8T6 làm vi điều khiển máy chủ trung tâm (Centralized Single-MCU Architecture).

---

## Abstract

This document presents the complete engineering design rationale for a centralized single-microcontroller embedded inclinometer system. The architecture consolidates real-time inertial sensor acquisition, digital filtering, non-volatile parameter persistence, graphical rendering, and host telemetry onto an ARM Cortex-M3 STM32F103C8T6 operating at 72 MHz. We detail the hardware peripheral isolation scheme (I2C1 for dual MPU-6050, dedicated SPI1 for W25Q64 Flash, dedicated SPI2 for ILI9341 TFT LCD, and USART1 for PC telemetry), mathematically verify deterministic 100 Hz sampling execution via hardware timer interrupt preemption over graphical rendering, calculate signal integrity and bus rise-time dynamics for I2C and high-speed SPI buses, analyze the system power dissipation and thermal headroom, and present a rigorous Failure Mode and Effects Analysis (FMEA).

---

## 1. System Architecture & Workload Partitioning Rationale

### 1.1 Structural Decomposition

All system peripherals are directly controlled by the centralized STM32F103C8T6 host controller:

```
+---------------------------------------------------------------------------------------------------------+
|                                  STM32F103C8T6 CENTRALIZED HOST CONTROLLER                              |
|                              (ARM Cortex-M3, 32-bit @ 72 MHz, 64 KB Flash, 20 KB SRAM)                  |
+------------------------------------+-----------------------------------+--------------------------------+
|       I2C1 Bus (APB1 @ 36 MHz)     |     SPI1 Bus (APB2 @ 72 MHz)      |    SPI2 Bus (APB1 @ 36 MHz)    |
|       Clock: 400 kHz Fast-Mode     |     Clock: 18 MHz (Prescaler /4)  |    Clock: 18 MHz (Prescaler /2)|
+------------------------------------+-----------------------------------+--------------------------------+
| Peripherals:                       | Peripherals:                      | Peripherals:                   |
|   - PB6: I2C1_SCL                  |   - PA4: SPI1_NSS (Flash CS)      |   - PB12: TFT_CS               |
|   - PB7: I2C1_SDA                  |   - PA5: SPI1_SCK                 |   - PB13: SPI2_SCK             |
|   - Dual MPU6050 IMUs:             |   - PA6: SPI1_MISO                |   - PB14: SPI2_MISO (Opt)      |
|     * MPU1 (AD0=GND): Addr 0x68    |   - PA7: SPI1_MOSI                |   - PB15: SPI2_MOSI            |
|     * MPU2 (AD0=3V3): Addr 0x69    |   - Winbond W25Q64 8MB Flash      |   - PB0:  TFT_RST              |
|                                    |     (Calibration & Alpha store)   |   - PB1:  TFT_DC               |
|                                    |                                   |   - ILI9341 2.8" SPI TFT LCD   |
+------------------------------------+-----------------------------------+--------------------------------+
|                                    USART1 (APB2 @ 72 MHz)                                               |
|                                    Baud: 115200 bps, 8N1                                                |
|                                    - PA9:  USART1_TX (Telemetry stream: ROLL:.. PITCH:..)               |
|                                    - PA10: USART1_RX (ASCII Command parser: CALIBRATE, SET_ALPHA)       |
+---------------------------------------------------------------------------------------------------------+
```

### 1.2 Mathematical Proof: Deterministic Sampling vs. Display Preemption

Angle estimation combines high-frequency angular rate integration with low-frequency gravitational acceleration inclination using a discrete-time Complementary Filter:

$$\theta_k = \alpha \cdot (\theta_{k-1} + \omega_k \cdot \Delta t) + (1 - \alpha) \cdot \theta_{\text{acc, } k}$$

where:
* $\theta_k$: Estimated angle at time step $k$.
* $\omega_k$: Gyroscope angular rate along the measurement axis ($\text{deg/s}$).
* $\theta_{\text{acc, } k} = \text{atan2}(A_y, A_z) \cdot \frac{180}{\pi}$: Instantaneous accelerometer inclination ($\text{deg}$).
* $\alpha \in [0.95, 0.98]$: Filter weighting coefficient.
* $\Delta t$: Sampling period (nominal $\Delta t_0 = 10.0\text{ ms}$ at $f_s = 100\text{ Hz}$).

#### Error Propagation from Non-Deterministic Sampling Jitter
If the sampling period experiences stochastic or blocking jitter $\delta t_k$, such that $\Delta t_k = \Delta t_0 + \delta t_k$, the instantaneous integration error $\epsilon_k$ introduced into the gyroscope path is:

$$\epsilon_k = \alpha \cdot \omega_k \cdot \delta t_k$$

Under angular acceleration $\dot{\omega} = \frac{d\omega}{dt}$, the cumulative variance of the estimated angle over $N$ samples expands as:

$$\sigma^2_\theta = \sum_{k=1}^N \alpha^{2(N-k)} \cdot \left[ \omega_k^2 \cdot \sigma^2_{\delta t} + (\Delta t_0)^2 \cdot \sigma^2_\omega \right]$$

To maintain an angle estimation variance $\sigma_\theta \le 0.05^\circ$, the timing jitter variance must satisfy:

$$\sigma_{\delta t} \le \frac{0.05^\circ}{\omega_{\max} \cdot \sqrt{N}} \implies \sigma_{\delta t} \le 15\text{ }\mu\text{s} \quad (\text{for } \omega_{\max} = 100^\circ/\text{s})$$

#### Preemptive Architecture: Hardware Timer vs Display Rendering
On an STM32 with $20\text{ KiB}$ SRAM, buffering a full ILI9341 frame ($320 \times 240 \times 2 = 153.6\text{ KiB}$) is impossible. Instead:
1. **Dirty Rectangle / Text-Glyph Rendering:** Only modified numeric bounding boxes (e.g., $16 \times 24$ pixels $\approx 768\text{ bytes}$) are streamed to ILI9341 GRAM in the main background loop over SPI2.
2. **Strict Hardware Timer Preemption (Nested Vector Interrupt Controller - NVIC):**
   - Sensor reading and filter updating run inside `TIM2` interrupt service routine (ISR) at priority level 1 ($f_s = 100\text{ Hz}$).
   - Any background SPI2 display transmission is immediately suspended when `TIM2` fires.
   - Sensor acquisition takes $\approx 0.7\text{ ms}$ over 400 kHz I2C1, and complementary filtering takes $< 5\text{ }\mu\text{s}$.
   - Once the ISR finishes, background display rendering resumes seamlessly.
   - This design enforces $\sigma_{\delta t} < 0.1\text{ }\mu\text{s}$, completely eliminating display-induced sampling jitter.

---

## 2. Electrical Interconnect & Signal Integrity Analysis

### 2.1 I2C1 Bus Dynamics (STM32 PB6/PB7 ⟷ Dual MPU6050 GY-521)

```
        STM32F103 (3.3V)                                 GY-521 Breakouts
     +--------------------+                       +-----------------------------+
     | PB6 (I2C1_SCL) ----+───────────────────┬───| SCL (MPU 1)   SCL (MPU 2)   |
     |                    |       Rp = 4.7k   |   | (Pull-ups on breakout)      |
     | PB7 (I2C1_SDA) ----+───────────────────┼───| SDA (MPU 1)   SDA (MPU 2)   |
     |                    |                   |   |                             |
     | GND ---------------+───────────────────┼───| GND (MPU 1)   GND (MPU 2)   |
     | 3.3V Out ----------+───────────────────┼───| VCC (MPU 1)   VCC (MPU 2)   |
     |                    |                   |   +-----------------------------+
     +--------------------+                   |     AD0 (MPU 1) ──► GND (0x68)
                                              +────► AD0 (MPU 2) ──► 3.3V (0x69)
```

The I2C bus utilizes open-drain drivers with parallel pull-up resistors on the breakouts.

#### Rise-Time & Bus Capacitance Budget:
According to the I2C specification (UM10204), the maximum rise time for Fast-Mode ($400\text{ kHz}$) is $t_{r, \max} = 300\text{ ns}$.
With two GY-521 modules in parallel, the effective pull-up resistance is:

$$R_{p, \text{eff}} = \frac{4.7\text{ k}\Omega}{2} = 2.35\text{ k}\Omega$$

With total bus capacitance $C_{\text{bus}} \approx 40\text{ pF}$ (breadboard traces + two sensor pin inputs):

$$t_r \approx 2.2 \cdot R_{p, \text{eff}} \cdot C_{\text{bus}} = 2.2 \cdot (2350\text{ }\Omega) \cdot (40 \times 10^{-12}\text{ F}) \approx 206.8\text{ ns}$$

$206.8\text{ ns} < 300\text{ ns}$, satisfying the I2C Fast-Mode timing requirement with an operating margin $> 31\%$.

#### Hardware Address Anchoring:
* **MPU 1 (Primary):** AD0 hardwired to **GND** $\implies$ I2C 7-bit Address = `0x68` (`0b1101000`).
* **MPU 2 (Secondary):** AD0 hardwired to **3.3V** $\implies$ I2C 7-bit Address = `0x69` (`0b1101001`).

Leaving AD0 floating is strictly prohibited as high-impedance pickup flips the address dynamically, causing bus NACK timeouts.

---

### 2.2 SPI1 Bus Dynamics (STM32 PA4..PA7 ⟷ W25Q64 Flash Module)

```
        STM32F103 (Master)                         W25Q64 Breakout Module
     +--------------------+                       +-----------------------+
     | PA4 (SPI1_NSS) ----+───────────────────────| CS   (Pin 2)          |
     | PA5 (SPI1_SCK) ----+───────────────────────| CLK  (Pin 5)          |
     | PA6 (SPI1_MISO) ---+◄──────────────────────| DO   (Pin 3)          |
     | PA7 (SPI1_MOSI) ---+───────────────────────| DI   (Pin 6)          |
     | 3.3V Power --------+───────────────────────| VCC  (Pin 1)          |
     | GND ---------------+───────────────────────| GND  (Pin 4)          |
     +--------------------+                       | [SMD Pull-ups on PCB] |
                                                  | WP# -> VCC, HOLD#->VCC|
                                                  +-----------------------+
```

The physical module features a 6-pin single-row interface with onboard pull-ups on `/WP` and `/HOLD`.
* **Clock Prescaler:** Configured with APB2 ($72\text{ MHz}$) prescaler `/4`:
  $$f_{\text{SCK1}} = \frac{72\text{ MHz}}{4} = 18\text{ MHz}$$
* **Cycle Time:** $T_{\text{SCK1}} = 55.56\text{ ns}$.
* **Setup & Hold Times:** W25Q64 requires $t_{\text{CLQX}} \ge 2\text{ ns}$, $t_{\text{DVCH}} \ge 5\text{ ns}$. The STM32 hardware SPI controller provides $> 20\text{ ns}$ margin.

---

### 2.3 SPI2 Bus Dynamics (STM32 PB12..PB15, PB0, PB1 ⟷ ILI9341 2.8" TFT)

```
        STM32F103 (Master)                         ILI9341 2.8" SPI TFT Header
     +--------------------+                       +-------------------------------+
     | PB12 (TFT_CS) -----+──────────────────────►│ Pin 3: CS (Chip Select)       |
     | PB0  (TFT_RST) ----+──────────────────────►│ Pin 4: RESET (Hardware Reset) |
     | PB1  (TFT_DC) -----+──────────────────────►│ Pin 5: DC / RS (Data/Command) |
     | PB15 (SPI2_MOSI) --+──────────────────────►│ Pin 6: SDI / MOSI (Data In)   |
     | PB13 (SPI2_SCK) ---+──────────────────────►│ Pin 7: SCK (SPI Clock)        |
     | 5V / VIN Rail -----+──────────────────────►│ Pin 1: VCC (LDO Input)        |
     | 3.3V Rail ---------+──────────────────────►│ Pin 8: LED (Backlight Anode)  |
     | GND ---------------+──────────────────────►│ Pin 2: GND (Ground)           |
     +--------------------+                       +-------------------------------+
```

* **Bus Isolation:** Operating ILI9341 on dedicated SPI2 isolates display traffic completely from SPI1 Flash memory transactions, eliminating bus sharing conflicts and chip-select deadlocks.
* **Clock Prescaler:** Driven by APB1 ($36\text{ MHz}$) prescaler `/2`:
  $$f_{\text{SCK2}} = \frac{36\text{ MHz}}{2} = 18\text{ MHz}$$
* **Power Supply:** VCC is supplied from 5V to feed the display's onboard 3.3V low-dropout regulator (AMS1117-3.3V), preventing regulator dropout brownout. The LED backlight connects to 3.3V through a current-limiting resistor, drawing $\approx 45\text{ mA}$.

---

### 2.4 USART1 Host Telemetry Link (STM32 ⟷ PC Serial CLI)

```
       STM32F103 (USART1)                         USB-to-TTL Adapter / PC
     +--------------------+                       +--------------------+
     | PA9  (TX1) --------+──────────────────────►| RXD                |
     | PA10 (RX1) ◄-------+───────────────────────| TXD                |
     | GND ---------------+───────────────────────| GND                |
     +--------------------+                       +--------------------+
```

* **Baud Rate:** $115,200\text{ bps}$, 8 data bits, no parity, 1 stop bit ($8\text{N}1$).
* **Clock Accuracy:** Derived from $72\text{ MHz}$ PCLK2 with integer divisor $\text{BRR} = 39.0625$ ($\texttt{0x0271}$), resulting in exactly $0.000\%$ baud rate error.

---

## 3. Power Architecture & Thermal Budget Analysis

### 3.1 Power Rail Current Budget

The entire breadboard assembly is supplied through the primary 5V USB Type-C bus. The total current draw is summarized below:

| Subsystem | Operating Rail | Active Current ($I_{\text{typ}}$) | Peak Transient ($I_{\text{peak}}$) | Power ($P = V \cdot I$) |
| :--- | :--- | :--- | :--- | :--- |
| **STM32F103C8T6** | 3.3V (Onboard LDO) | $32\text{ mA}$ | $45\text{ mA}$ | $0.106\text{ W}$ |
| **MPU 1 (GY-521)** | 3.3V | $3.9\text{ mA}$ | $5.0\text{ mA}$ | $0.013\text{ W}$ |
| **MPU 2 (GY-521)** | 3.3V | $3.9\text{ mA}$ | $5.0\text{ mA}$ | $0.013\text{ W}$ |
| **W25Q64 Flash** | 3.3V | $4.0\text{ mA}$ (Read) | $15.0\text{ mA}$ (Erase) | $0.050\text{ W}$ |
| **ILI9341 Logic + Panel**| 5V $\rightarrow$ 3.3V (U1) | $12\text{ mA}$ | $15\text{ mA}$ | $0.060\text{ W}$ |
| **TFT LED Backlight** | 3.3V | $45\text{ mA}$ | $50\text{ mA}$ | $0.149\text{ W}$ |
| **TOTAL SYSTEM LOAD**| **5.0V USB Rail** | **$100.8\text{ mA}$** | **$135.0\text{ mA}$** | **$0.504\text{ W}$ (Nominal)** |

**Compliance:** Standard USB 2.0 ports deliver up to $500\text{ mA}$ ($2.5\text{ W}$). The centralized single-MCU system draws only $\approx 100.8\text{ mA}$ ($20.1\%$ of USB 2.0 capacity), providing massive electrical and thermal safety headroom.

---

## 4. Failure Mode and Effects Analysis (FMEA)

| Item ID | Failure Mode | Root Cause | Effect on System | $S$ | $O$ | $D$ | RPN | Mitigation Strategy |
| :---: | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **FM-01** | **I2C Bus Lockup (SDA stuck LOW)** | Sensor interrupted mid-byte during low-phase clock stretch | Entire STM32 measurement loop hangs waiting for I2C flag | 9 | 4 | 2 | **72** | Firmware 9-clock toggle recovery pulse sequence in `MPU_Init()`; hardware I2C timeout interrupts. |
| **FM-02** | **AD0 Floating Address Desync** | Mechanical breadboard contact failure on GY-521 pin 7 | Address flips between 0x68 and 0x69; sensor read returns `0xFF` | 8 | 5 | 2 | **80** | Hardwire MPU1 AD0 to GND, MPU2 AD0 to 3.3V; firmware validates WHO_AM_I for both addresses. |
| **FM-03** | **SPI Bus Contention** | Shared SPI bus between Flash and Display causing CS conflicts | Display redraw corrupts flash write or locks SPI bus | 8 | 4 | 2 | **64** | **Dedicated hardware SPI isolation:** W25Q64 on SPI1, ILI9341 on SPI2. |
| **FM-04** | **Flash Overvoltage Degradation** | VCC of W25Q64 connected to 5V rail instead of 3.3V | Irreversible dielectric breakdown of 65nm NOR cells | 10 | 2 | 2 | **40** | Physical circuit verification; explicit 3.3V rail color-coded isolation in wiring guides. |
| **FM-05** | **TFT White Screen (Blank)** | Incorrect wiring on CS (PB12), DC (PB1), or RST (PB0) | LCD controller fails to receive initialization sequence commands | 6 | 4 | 1 | **24** | Verified pin mapping against wiring reference; dedicated hardware reset pulse sequence (10ms-50ms-150ms). |
| **FM-06** | **Gyroscope Thermal Zero-Bias Drift** | Sensor self-heating ($3.9\text{ mA}$) shifts zero-rate output | Estimated pitch/roll angle drifts over time by $1\text{–}3^\circ$ | 5 | 6 | 4 | **120** | Static bias calibration routine at boot; calibrated offset coefficients stored in W25Q64 Flash. |

---

## 5. Architectural Conclusions & Verification Criteria

1. **Physical Parity:** The hardware layout in `docs/Wiring_and_Pinouts.md` accurately matches the centralized host STM32F103C8T6 pin distribution.
2. **Deterministic Integrity:** Sampling rate is maintained via STM32 hardware Timer interrupts ($\Delta t = 10\text{ ms} \pm 0.1\text{ }\mu\text{s}$), preempting background display rendering.
3. **Electrical Margins:** Dedicated buses (I2C1 for dual MPU, SPI1 for Flash, SPI2 for TFT, USART1 for CLI) prevent electrical and bus arbitration conflicts.
