# UART Commands & Operational Reference

Complete reference of all UART commands supported across the repository firmware, including the **5-LED Lab Control Firmware** and the **Production Inclinometer Telemetry Firmware**.

---

## 1. 5-LED Control Lab Commands (`starter/uart/ex05_*.c`, `ex06_*.c`)

* **Baud Rate:** **9600 Baud**
* **Framing:** 8 Data Bits, No Parity, 1 Stop Bit (8-N-1)
* **Termination:** Enter (`\r`, `\n`) or Exclamation mark (`!`)
* **Hardware Pins:** `PA0` (LED1), `PA1` (LED2), `PA2` (LED3), `PA3` (LED4), `PA4` (LED5) — Active-High.
* **UART Pins:** `PA9` (USART1_TX $\to$ PC RXD), `PA10` (USART1_RX $\leftarrow$ PC TXD).

| Command String | Pin Affected | Action Description | Microcontroller Response |
|---|---|---|---|
| `LED1ON` | `PA0` | Sets PA0 HIGH (turns LED1 ON) | `LED1: ON\r\n` |
| `LED1OFF` | `PA0` | Sets PA0 LOW (turns LED1 OFF) | `LED1: OFF\r\n` |
| `LED2ON` | `PA1` | Sets PA1 HIGH (turns LED2 ON) | `LED2: ON\r\n` |
| `LED2OFF` | `PA1` | Sets PA1 LOW (turns LED2 OFF) | `LED2: OFF\r\n` |
| `LED3ON` | `PA2` | Sets PA2 HIGH (turns LED3 ON) | `LED3: ON\r\n` |
| `LED3OFF` | `PA2` | Sets PA2 LOW (turns LED3 OFF) | `LED3: OFF\r\n` |
| `LED4ON` | `PA3` | Sets PA3 HIGH (turns LED4 ON) | `LED4: ON\r\n` |
| `LED4OFF` | `PA3` | Sets PA4 LOW (turns LED4 OFF) | `LED4: OFF\r\n` |
| `LED5ON` | `PA4` | Sets PA4 HIGH (turns LED5 ON) | `LED5: ON\r\n` |
| `LED5OFF` | `PA4` | Sets PA4 LOW (turns LED5 OFF) | `LED5: OFF\r\n` |
| `LEDONALL` | `PA0`..`PA4` | Turns all 5 LEDs ON simultaneously | `LED ALL: ON\r\n` |
| `LEDOFFALL` | `PA0`..`PA4` | Turns all 5 LEDs OFF simultaneously | `LED ALL: OFF\r\n` |
| *(Any Invalid)* | — | Unknown command rejected | `ERROR\r\n` |

---

## 2. Production Inclinometer Telemetry Commands (`src/lib/shared/uart_cmd.c`)

* **Baud Rate:** **115,200 Baud**
* **Framing:** 8 Data Bits, No Parity, 1 Stop Bit (8-N-1)
* **Termination:** Carriage Return + Line Feed (`\r\n` / CRLF)
* **UART Pins:** `PA9` (USART1_TX), `PA10` (USART1_RX).

| Command String | Parameter Format | Action Description | Expected Response Example |
|---|---|---|---|
| `INFO_REQUEST` | None | Queries group metadata & hardware BOM | `GROUP_NAME: Group 04 - MPU6050 Inclinometer\r\nMEMBERS: ...\r\n` |
| `INFO_REQUEST:GROUP` | None | Alias for group information query | Same as `INFO_REQUEST` |
| `GET_ANGLE` | None | Returns instantaneous Roll and Pitch angles | `ROLL:12.3\r\nPITCH:-4.5\r\n` |
| `CALIBRATE_IMU` | None | Executes static 200-sample sensor calibration and saves zero-offsets to SPI Flash | `CALIBRATE:OK\r\nACC_OFFSET:0.0,0.0,1.0\r\nGYRO_OFFSET:0.0,0.0,0.0\r\n` |
| `SET_FILTER_ALPHA:<val>` | Float (e.g. `0.96`) | Sets complementary filter coefficient $\alpha$ ($0.0 < \alpha < 1.0$) and commits to Flash | `FILTER_ALPHA:0.96\r\n` |

---

## 3. Host Build & Flashing Commands

### Build & Flash LED Lab to Hardware (ST-Link V2)
```bash
cd /home/vgbn/Documents/codeptit/stm32-esp32/starter/uart

# Polling implementation:
make SRC=ex05_led_control_polling.c flash

# Interrupt implementation:
make SRC=ex06_led_control_interrupt.c flash
```

### Build & Flash Production Inclinometer Firmware
```bash
cd /home/vgbn/Documents/codeptit/stm32-esp32/BTL_KTVXL

# Build unified firmware
make

# Flash via ST-Link
make flash
```

---

## 4. Terminal Emulator Launch Commands

### Linux Terminal
```bash
# All-in-one interactive launcher (Hercules clone / Picocom / PuTTY)
./scripts/linux/launch_serial.sh

# Direct Picocom for 9600 Baud LED Lab:
picocom -b 9600 /dev/ttyUSB0 --echo --imap lfcrlf

# Direct Picocom for 115200 Baud Inclinometer:
picocom -b 115200 /dev/ttyUSB0 --echo --imap lfcrlf
```

### Windows PuTTY CLI Shortcut
```cmd
# 9600 Baud (LED Lab):
putty.exe -serial COM3 -sercfg 9600,8,n,1,N

# 115200 Baud (Inclinometer):
putty.exe -serial COM3 -sercfg 115200,8,n,1,N
```

### Windows HW-Group Hercules Macros
In Hercules "Serial" tab, set `Baud: 9600` or `115200` and configure Transmit buttons:
- Macro 1: `LEDONALL$0D$0A`
- Macro 2: `LEDOFFALL$0D$0A`
- Macro 3: `GET_ANGLE$0D$0A`
- Macro 4: `INFO_REQUEST$0D$0A`
- Macro 5: `CALIBRATE_IMU$0D$0A`
