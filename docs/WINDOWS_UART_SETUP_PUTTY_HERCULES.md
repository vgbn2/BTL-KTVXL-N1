# Production-Grade Windows UART Telemetry Operational Setup Guide: PuTTY & HW-Group Hercules

**Target Hardware:** STM32F103C8T6 (ARM Cortex-M3 "Blue Pill" / Custom Inclinometer Board)  
**Peripheral:** USART1 (`PA9` = TX, `PA10` = RX)  
**Operating System:** Windows 10 / Windows 11 (64-bit)  
**Document Code:** `DOC-OPER-UART-WIN-01`  
**Revision:** v1.0 (2026-11-08)

---

## 1. Causal Architecture & Physical Layer Specification

### 1.1 Physical & Protocol Invariants
All UART telemetry communications between the host PC and the STM32F103 USART1 engine operate under strict 8-N-1 framing:

| Parameter | Specification | Physical / Firmware Invariant |
| :--- | :--- | :--- |
| **Baud Rate** | **`115200` bps** | Calculated via $f_{\text{PCLK2}} = 72\text{ MHz} \implies \text{USARTDIV} = 39.0625 \implies \text{BRR} = \text{0x0271}$ ($0.00\%$ error). |
| **Data Bits** | **`8`** | Standard 8-bit ASCII payload (`USART_WordLength_8b`). |
| **Parity** | **`None`** | Parity checking disabled (`USART_Parity_No`). |
| **Stop Bits** | **`1`** | Single stop bit framing (`USART_StopBits_1`). |
| **Flow Control** | **`None`** | No hardware RTS/CTS lines (`USART_HardwareFlowControl_None`). |
| **Command Delimiter**| **`\r\n` (CRLF) or `\n` (LF)** | Firmware parser strips trailing `\r` and `\n` characters before string evaluation. |
| **RX Ingestion** | **Interrupt + IDLE Line** | Bytes are received in `USART1_IRQHandler` via `RXNE`; command boundary is latched on `USART_IT_IDLE`. |

### 1.2 The Underlying Causal Mechanics (Why Things Break)
1. **Crossed RX/TX Transmission:**
   The host USB-UART adapter's `TX` pin transmits data bits that must be physically routed to the STM32's `RX` pin (`PA10`). Reciprocally, the STM32's `TX` pin (`PA9`) transmits responses that must enter the host's `RX` pin. If connected TX-to-TX and RX-to-RX, both transmitters will drive against each other with zero signal reception.
2. **Mandatory Common Reference Ground (GND):**
   UART is an unshielded, single-ended asynchronous communication bus. Logic `1` (Mark) is $+3.3\text{V}$ and Logic `0` (Space) is $0\text{V}$ *relative to circuit ground*. Without a direct, low-impedance GND tie between the USB-UART adapter and the STM32, floating voltage potentials cause random logic threshold crossings, manifesting as framing errors, parity flags, and garbage bytes (`0xFF`, `0x00`).
3. **Local Echo & Line Buffering Necessity in PuTTY:**
   The STM32 firmware (`uart_cmd.c`) is designed as a clean command receiver: it buffers incoming bytes silently in a 128-byte buffer and executes only upon receiving a complete command (or line IDLE). It **does not echo back individual characters** as you type them. If terminal "Local echo" is disabled, typing into PuTTY appears completely dead. Similarly, "Local line editing" allows the host terminal to buffer and edit the command line before transmitting it upon hitting `<Enter>`.

---

## 2. Hardware Wiring & USB-to-UART Adapter Setup

### 2.1 Wiring Pinout Table

```
+------------------------------------+           +----------------------------+
|        STM32F103C8T6 Board         |           |   USB-to-UART Bridge       |
|                                    |           |   (CH340 / CP2102 / FT232) |
|   PA9  [USART1_TX] ----------------+-----------> RXD / RX                   |
|   PA10 [USART1_RX] <---------------+-----------+ TXD / TX                   |
|   GND              <===========================> GND (MANDATORY COMMON GND)  |
|   5V / 3.3V (Opt)  <---------------------------+ VCC (Ensure jumper @ 3.3V) |
+------------------------------------+           +----------------------------+
```

| STM32 Pin | Function | Connect to USB-UART Adapter | Notes & Electrical Precautions |
| :--- | :--- | :--- | :--- |
| **`PA9`** | `USART1_TX` | **`RX` / `RXD`** | STM32 output driving host input ($3.3\text{V}$ Push-Pull). |
| **`PA10`** | `USART1_RX` | **`TX` / `TXD`** | STM32 input receiving host commands ($5\text{V}$-tolerant floating input). |
| **`GND`** | Ground | **`GND`** | **MANDATORY.** Common electrical reference potential. |
| **`5V` or `3.3V`**| Power (Optional) | **`VCC`** | If powering STM32 from USB-UART, verify adapter jumper is set to $3.3\text{V}$ logic. If STM32 is powered via onboard USB / ST-Link, **leave VCC disconnected**! |

> ⚠️ **CAUTION:** Never connect a $5\text{V}$ VCC supply to the STM32 $3.3\text{V}$ header pin! This will destroy the STM32F103 MCU and MPU6050 sensor. If the STM32 is already powered via its Micro-USB connector or ST-Link SWD probe, connect **only TX, RX, and GND**.

---

### 2.2 Adapter Driver Guide for Windows

| Chipset Family | Manufacturer | USB VID:PID | Official Windows Driver |
| :--- | :--- | :--- | :--- |
| **CH340 / CH341** | WCH (Nanjing Qinheng) | `1A86:7523` | [WCH CH341SER.EXE](http://www.wch-ic.com/downloads/CH341SER_EXE.html) |
| **CP2102 / CP2104** | Silicon Labs | `10C4:EA60` | [Silicon Labs CP210x Universal Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| **FT232R / FT232RL**| FTDI Chip | `0403:6001` | [FTDI VCP Drivers](https://ftdichip.com/drivers/vcp-drivers/) |
| **ST-Link VCP** | STMicroelectronics | `0483:374B` | Included in ST-Link utility / STM32CubeProgrammer |

---

### 2.3 Identifying the Assigned COM Port on Windows

#### Method A: Device Manager GUI
1. Press `Win + R`, type `devmgmt.msc`, and press `Enter`.
2. Expand the **Ports (COM & LPT)** branch.
3. Locate your adapter:
   - `USB-SERIAL CH340 (COM3)`
   - `Silicon Labs CP210x USB to UART Bridge (COM5)`
   - `USB Serial Port (COM4)`
   - `STMicroelectronics Virtual COM Port (COM6)`
4. Note down the port identifier (e.g. `COM3`).

```
[Device Manager]
 └── Ports (COM & LPT)
      ├── USB-SERIAL CH340 (COM3) <--- Your target port
      └── Communications Port (COM1)
```

#### Method B: PowerShell Quick Probe (Zero-Install)
Open PowerShell (`Win + X` -> `Terminal` or `powershell`) and run:

```powershell
# Fast query returning all active COM port strings
[System.IO.Ports.SerialPort]::GetPortNames()

# Detailed query showing device description and hardware status
Get-PnpDevice -Class Ports -Present | Select-Object FriendlyName, InstanceId
```

*Expected Healthy Output:*
```text
FriendlyName                                InstanceId
------------                                ----------
USB-SERIAL CH340 (COM3)                     USB\VID_1A86&PID_7523\5&2C6D13B&0&2
```

---

## 3. PuTTY Setup & Operational Configuration

PuTTY is a standard, lightweight terminal emulator. Because embedded microcontrollers interact differently than Linux SSH servers, specific terminal emulation flags must be set.

### 3.1 Step-by-Step GUI Setup

#### Step 1: Session & Connection Type
1. Launch `putty.exe`.
2. Under **Connection type**, click the **Serial** radio button.
3. In the **Serial line** field, enter your COM port (e.g. `COM3`).
4. In the **Speed** field, enter **`115200`**.

```
+-----------------------------------------------------------+
| PuTTY Configuration                                       |
|-----------------------------------------------------------|
| Category:                                                 |
|   Session           Specify the destination you want to...|
|   ...               Connection type:                      |
|   Connection         ( ) Raw  ( ) Telnet  ( ) Rlogin      |
|     ...              ( ) SSH  (*) Serial                  |
|     Serial                                                |
|                     Serial line: [ COM3       ]           |
|                     Speed:       [ 115200     ]           |
+-----------------------------------------------------------+
```

#### Step 2: Serial Port Parameters
1. In the left Category tree, navigate to **Connection** > **Serial**.
2. Configure the exact framing:
   - **Serial line to connect to:** `COM3` (match your port)
   - **Speed (baud):** `115200`
   - **Data bits:** `8`
   - **Stop bits:** `1`
   - **Parity:** `None`
   - **Flow control:** `None`

#### Step 3: Terminal Emulation Flags (Recommended)
1. In the left Category tree, navigate to **Terminal**.
2. Keep the following settings:
   - **Implicit CR in every LF:** Check this box `[X]` (Prevents stair-stepped line printing if a line arrives with only `\n`).
   - **Implicit LF in every CR:** Leave unchecked `[ ]`.
   - **Local echo:** Leave at **`Auto`** (STM32 firmware echoes typed characters and `\r\n`; selecting `Force on` will double-echo).
   - **Local line editing:** Leave at **`Auto`** (STM32 firmware handles `\b` / `0x7F` Backspace and `Ctrl+C` directly).

```
+-----------------------------------------------------------+
| Category: Terminal                                        |
|   Line discipline options:                                |
|     Local echo:          (*) Auto   ( ) Force off  ( ) Force on |
|     Local line editing:  (*) Auto   ( ) Force off  ( ) Force on |
|   Terminal character options:                             |
|     [X] Implicit CR in every LF                           |
|     [ ] Implicit LF in every CR                           |
+-----------------------------------------------------------+
```

#### Step 4: Save the Session Profile
1. In the Category tree, click back on **Session** (top item).
2. In the **Saved Sessions** text box, type:
   ```text
   STM32_MPU6050_115200
   ```
3. Click the **Save** button.
4. From now on, simply double-click `STM32_MPU6050_115200` or select it and click **Open**.

---

### 3.2 PuTTY Windows Command-Line Shortcut

To bypass the GUI entirely and launch an instant terminal from CMD, PowerShell, or Desktop shortcut:

```cmd
putty.exe -serial COM3 -sercfg 115200,8,n,1,N
```

#### Parameter Breakdown:
- `-serial <port>`: Specifies serial connection on `COM3`.
- `-sercfg <baud>,<bits>,<parity>,<stop>,<flow>`:
  - `115200`: Baud rate.
  - `8`: 8 Data bits.
  - `n`: None parity (`n` = none, `e` = even, `o` = odd).
  - `1`: 1 Stop bit.
  - `N`: No flow control (`N` = none, `X` = XON/XOFF, `R` = RTS/CTS, `D` = DSR/DTR).

#### Ready-to-Use Launcher Script:
Use the included automated batch launcher at:
```cmd
scripts\windows\launch_putty.bat
```
*(This script scans Windows for active COM ports, lets you select one, and launches PuTTY automatically).*

---

## 4. HW-Group Hercules Setup & Transmit Macros

HW-Group Hercules is an industry-standard, zero-install portable diagnostic tool (`hercules.exe`) designed specifically for testing serial microcontrollers and field devices.

### 4.1 "Serial" Tab Port Configuration

1. Launch `hercules.exe` and select the **Serial** tab.
2. In the right-hand panel, configure the Serial parameters:
   - **Name:** Select your COM port (e.g. `COM3`).
   - **Baud:** `115200`.
   - **Data size:** `8`.
   - **Parity:** `none`.
   - **Handshake:** `OFF`.
   - **Mode:** `Free`.
3. Click the **Open** button.
4. The status bar at the bottom will display:  
   `Serial port COM3 opened 115200, 8, N, 1, Handshake: None`

```
+-------------------------------------------------------------------------------+
|  Hercules SETUP utility by HW-group.com                                       |
|  [UDP]  [Serial]  [TCP Client]  [TCP Server]  [Test Mode]                     |
|-------------------------------------------------------------------------------|
|  Receive Terminal Window                        |  Serial Port Settings       |
|                                                 |  Name:      [ COM3      v ] |
|                                                 |  Baud:      [ 115200    v ] |
|                                                 |  Data size: [ 8         v ] |
|                                                 |  Parity:    [ none      v ] |
|                                                 |  Handshake: [ OFF       v ] |
|                                                 |  Mode:      [ Free      v ] |
|                                                 |  [ Open ]  [ Close ]        |
+-------------------------------------------------------------------------------+
```

---

### 4.2 Transmit Macro Configuration (With CRLF Formatting)

Hercules supports sending predefined strings using its numbered Macro buttons (**Macro 1** to **Macro 5**).  
In Hercules, non-printable characters are encoded with a dollar sign prefix:  
- Carriage Return (`\r` = ASCII `0x0D`): **`$0D`**
- Line Feed (`\n` = ASCII `0x0A`): **`$0A`**

Configure the fields under **Send** in the lower panel as follows:

| Button # | Label Field | Command String (Send Field) | Operation Triggered |
| :---: | :--- | :--- | :--- |
| **M1** | `GET_ANGLE` | `GET_ANGLE$0D$0A` | Requests instantaneous Roll & Pitch angles. |
| **M2** | `INFO_REQ` | `INFO_REQUEST$0D$0A` | Queries group roster, firmware, and project metadata. |
| **M3** | `CALIB_IMU` | `CALIBRATE_IMU$0D$0A` | Initiates 200-sample static calibration and Flash save. |
| **M4** | `ALPHA_0.96`| `SET_FILTER_ALPHA:0.96$0D$0A` | Sets complementary filter coefficient $\alpha = 0.96$. |
| **M5** | `ALPHA_0.98`| `SET_FILTER_ALPHA:0.98$0D$0A` | Sets complementary filter coefficient $\alpha = 0.98$ (Default).|

*Screenshot Equivalent Setup:*
```text
Macro 1: [ GET_ANGLE$0D$0A             ]  [ Send ]  Label: GET_ANGLE
Macro 2: [ INFO_REQUEST$0D$0A          ]  [ Send ]  Label: INFO_REQ
Macro 3: [ CALIBRATE_IMU$0D$0A         ]  [ Send ]  Label: CALIB_IMU
Macro 4: [ SET_FILTER_ALPHA:0.96$0D$0A ]  [ Send ]  Label: ALPHA_0.96
Macro 5: [ SET_FILTER_ALPHA:0.98$0D$0A ]  [ Send ]  Label: ALPHA_0.98
```

---

### 4.3 HEX Display Mode & Telemetry Logging

#### Enabling HEX Display Mode:
To verify raw framing bytes (such as checking if the STM32 sends `0x0D 0x0A` or inspecting binary sensor packets):
1. Right-click anywhere in the Hercules **Receive** text pane.
2. Check the **HEX** checkbox in the context menu.
3. The display will format received bytes in hexadecimal side-by-side with ASCII:
   ```text
   52 4F 4C 4C 3A 30 2E 30 0D 0A 50 49 54 43 48 3A 30 2E 30 0D 0A
   ROLL:0.0..PITCH:0.0..
   ```

#### Logging Telemetry to File:
1. Right-click in the **Receive** text pane.
2. Select **Log to file...**.
3. Choose a destination filename (e.g. `telemetry_run1.log`).
4. All incoming angle data and responses will be continuously appended to disk.

---

## 5. Firmware Command Specification & Expected Responses

The STM32F103 firmware implements an interrupt-driven command state machine in `src/lib/shared/uart_cmd.c`:

### 5.1 Command Dictionary

#### 1. `INFO_REQUEST` or `INFO_REQUEST:GROUP`
- **Purpose:** Verification of team members, project specifications, and firmware build date.
- **Payload:** `INFO_REQUEST\r\n`
- **Expected STM32 Response:**
  ```text
  GROUP_NAME: Group 04 - MPU6050 Inclinometer
  MEMBERS: Tran Duc Anh, Tran Quoc An, Nguyen Duy Anh, Tran Tuan Anh, Tran Khac Anh
  PROJECT_NAME: Inclinometer Device Using MPU6050
  HARDWARE_USED: STM32, MPU6050, OLED/TFT, W25Qxx
  SOFTWARE_USED: Keil MDK, SPL
  DATE: 2026-10-26
  ```

#### 2. `GET_ANGLE`
- **Purpose:** Polled reading of current Roll and Pitch angles calculated by the Complementary Filter.
- **Payload:** `GET_ANGLE\r\n`
- **Expected STM32 Response:**
  ```text
  ROLL:12.4
  PITCH:-5.8
  ```
  *(Format: `ROLL:<whole>.<frac>\r\nPITCH:<whole>.<frac>\r\n`)*

#### 3. `CALIBRATE_IMU`
- **Purpose:** Samples the IMU in a static position, computes offsets (subtracting $+1.0\text{g}$ from the Z-axis), and saves parameters to W25Qxx SPI Flash.
- **Payload:** `CALIBRATE_IMU\r\n`
- **Expected STM32 Response (Success):**
  ```text
  CALIBRATE:OK
  ACC_OFFSET:0.0,0.0,0.0
  GYRO_OFFSET:0.0,0.0,0.0
  ```
- **Expected STM32 Response (Failure / I2C Error):**
  ```text
  ERROR:CALIBRATE_FAILED
  ```

#### 4. `SET_FILTER_ALPHA:<val>`
- **Purpose:** Dynamically adjust the Complementary Filter coefficient $\alpha$ (valid range: $0.50$ to $0.999$) and persist to Flash.
- **Payload:** `SET_FILTER_ALPHA:0.96\r\n`
- **Expected STM32 Response (Success):**
  ```text
  FILTER_ALPHA:0.9
  ```
- **Expected STM32 Response (Invalid range):**
  ```text
  ERROR:INVALID_ALPHA
  ```

#### 5. Unknown Command
- **Payload:** Any unregistered string (e.g. `TEST\r\n`)
- **Expected STM32 Response:**
  ```text
  ERROR:UNKNOWN_CMD
  ```

---

## 6. Native Windows Stdlib / Zero-Install Alternatives

When working on laboratory workstations where installing third-party utilities (PuTTY/Hercules) is restricted, use these zero-install alternatives.

### 6.1 Topic Runbook: Windows Native Serial Telemetry

#### 1. Inspection & Diagnostic Probes (Read-Only)
```powershell
# Enumerate all serial ports recognized by Windows .NET subsystem
[System.IO.Ports.SerialPort]::GetPortNames()

# Probe hardware driver state for Port class devices
Get-PnpDevice -Class Ports -Present | Format-Table -AutoSize FriendlyName, Status, Class
```
*Expected Healthy Signature:* `Status: OK`, `FriendlyName: USB-SERIAL CH340 (COMx)`.  
*Failure Signature:* Yellow exclamation mark in Device Manager or empty list.

#### 2. Remediation & Action: Native PowerShell Serial Terminal
Run the included native zero-dependency terminal script:
```powershell
# Standard launch with auto-discovery
powershell -ExecutionPolicy Bypass -File scripts\windows\serial_terminal.ps1

# Or specify port explicitly
powershell -ExecutionPolicy Bypass -File scripts\windows\serial_terminal.ps1 -Port COM3
```

#### 3. Verification & Oracle Gate
```powershell
# In the terminal prompt, type:
1
# (Triggers INFO_REQUEST)
```
*Expected Output:*
```text
[HH:mm:ss.fff TX] INFO_REQUEST
[HH:mm:ss.fff RX] GROUP_NAME: Group 04 - MPU6050 Inclinometer
...
```

#### 4. Rollback & Emergency Exit
To close the serial port and return to system prompt, press `q` in the terminal menu or hit `Ctrl+C`. The script's `finally` block guarantees `$serial.Close()` and `$serial.Dispose()`.

---

### 6.2 Python Cross-Platform Monitor (`scripts/windows/monitor_uart.py`)

If Python is available with `pyserial`:

```bash
# Install dependency if needed
pip install pyserial

# Run interactive monitor
python scripts/windows/monitor_uart.py

# Optional: Log continuous telemetry to CSV
python scripts/windows/monitor_uart.py -p COM3 -l telemetry.csv
```

Features:
- Live background reception while typing commands.
- Colorized timestamped console output (`[TX]` in yellow, `[RX]` in green).
- Pre-mapped keyboard shortcuts (`1` for `INFO_REQUEST`, `2` for `GET_ANGLE`, etc.).

---

## 7. Troubleshooting & Root-Cause Failure Matrix

| Symptom / Observation | Underlying Causal Root Cause | Exact Remediation Step |
| :--- | :--- | :--- |
| **PuTTY shows "Unable to open serial port"** | Port is locked exclusively by another process (Arduino IDE, Hercules, Python script, or previous PuTTY session). | Close all other terminal programs. Run `Get-Process \| Where-Object { $_.MainWindowTitle -match 'PuTTY' } \| Stop-Process` in PowerShell. |
| **PuTTY window opens, but typing produces no characters on screen** | Firmware baud mismatch, TX line disconnected (`PA9` -> USB RX), or older non-echo firmware build. | Verify `115200 8-N-1`, `PA9` -> adapter RX, and press `NRST` to check for the `MPU6050 Inclinometer READY` boot banner (leave `Local echo` at `Auto` on current firmware). |
| **Garbage / unreadable characters (`\x00\xFF`) printed** | 1. Baud rate mismatch (e.g. 9600 vs 115200).<br>2. Missing common GND connection causing floating voltage logic levels.<br>3. Inverted logic or noisy jumper wires. | 1. Verify baud is **`115200`** in terminal.<br>2. Confirm **GND wire** connects USB-UART adapter GND directly to STM32 GND.<br>3. Replace breadboard jumper wires with short (<15 cm) ribbon cables. |
| **Commands sent, but STM32 never responds** | 1. `TX` and `RX` lines are swapped.<br>2. Missing `\r\n` line terminator (STM32 IDLE interrupt did not trigger or parser did not find delimiter).<br>3. STM32 in HardFault or halted in debugger. | 1. Swap `TX` and `RX`: ensure Host `TX` -> STM32 `PA10` (`RX`), and Host `RX` -> STM32 `PA9` (`TX`).<br>2. In Hercules, verify command has `$0D$0A` suffix.<br>3. Press the black `NRST` button on the STM32 board to reboot firmware. |
| **Staircase text effect in terminal (each line indents further right)** | Firmware outputs newline `\n` without preceding carriage return `\r`, or terminal treats `\n` as line feed only. | In PuTTY: check **Terminal** > **Implicit CR in every LF**. |
| **Device Manager shows "Unknown Device" (Code 28)** | USB-to-UART chipset driver is missing in Windows. | Identify the chip (CH340, CP2102, or FT232) and install the official manufacturer driver from Section 2.2. |
| **Calibration returns `ERROR:CALIBRATE_FAILED`** | MPU6050 sensor failed I2C communication during static sample collection. | Check I2C wiring between STM32 (`PB6` SCL, `PB7` SDA) and MPU6050. Ensure pull-up resistors are present and $3.3\text{V}$ power is stable. |

---

## 8. Summary Checklist for Lab Demonstration

- [ ] USB-UART Adapter GND connected to STM32 GND.
- [ ] USB-UART TX connected to STM32 `PA10` (RX).
- [ ] USB-UART RX connected to STM32 `PA9` (TX).
- [ ] Windows Device Manager confirms COM port number (e.g. `COM3`).
- [ ] Terminal set to: **115200 Baud, 8 Data bits, None parity, 1 Stop bit, No flow control**.
- [ ] PuTTY left at **Local echo: Auto**, **Local line editing: Auto**, and **Implicit CR in every LF: Checked**.
- [ ] Hercules Macros loaded with `GET_ANGLE$0D$0A` and `INFO_REQUEST$0D$0A`.
- [ ] Test command `INFO_REQUEST` returns Group 04 metadata.
- [ ] Test command `GET_ANGLE` returns active roll and pitch angles.
