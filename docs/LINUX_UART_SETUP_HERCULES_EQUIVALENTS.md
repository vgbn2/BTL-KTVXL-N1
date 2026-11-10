# Linux Serial Telemetry & UART Terminal Guide (Hercules & PuTTY Equivalents)

This guide provides operational workflows on **Linux** to connect to the STM32F103 telemetry console (`USART1`: PA9 TX, PA10 RX @ 115200 8-N-1, CRLF).

---

## 1. Physical Device Permissions & Discovery

On Linux, USB-to-UART bridges (CH340, CP2102, FT232, ST-Link VCP) appear as character devices under `/dev/`:
- `/dev/ttyUSB0` (FTDI, CP210x, CH34x)
- `/dev/ttyACM0` (ST-Link Virtual COM Port, CDC-ACM)

### 1.1 Permission Setup (Dialout Group)
By default, serial ports are owned by `root:dialout`. If your user lacks access, grant permissions once:
```bash
sudo usermod -aG dialout $USER
# Activate immediately in current terminal:
newgrp dialout
```

### 1.2 Inspection Command
```bash
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
dmesg | grep -E "tty(USB|ACM)|ch341|cp210x|ftdi"
```

---

## 2. Linux Equivalents to Windows Tools

| Windows Tool | Direct Linux Equivalent | Interface | Key Features |
|---|---|---|---|
| **HW-Group Hercules** | **CuteCom** (`sudo apt install cutecom`) | GUI | Dedicated macro list, hex/ASCII views, CRLF selector |
| **PuTTY (Windows)** | **PuTTY (Linux)** (`sudo apt install putty`) | GUI | 100% identical configuration menus & CLI flags |
| **Hercules Macros** | **`scripts/linux/launch_serial.sh` (Option 1)** | CLI | Built-in numeric keys `[1]` to `[5]` for STM32 commands |
| **Lightweight CLI** | **`picocom`** (Pre-installed) | CLI | Zero-install, instantaneous launch, low overhead |

---

## 3. Tool Configurations

### A. All-in-One Launcher Script
Located at:
```bash
./scripts/linux/launch_serial.sh
```
Presents a simple menu:
1. Python interactive shell with `[1]`–`[5]` Hercules-style macro shortcuts and colorized output.
2. `picocom` CLI session with local echo and CRLF mapping.
3. `cutecom` GUI (Hercules twin).
4. `putty` GUI.

---

### B. CuteCom (The 1:1 Hercules GUI Twin)
1. **Install:**
   ```bash
   sudo apt install -y cutecom
   ```
2. **Launch:**
   ```bash
   cutecom &
   ```
3. **Settings in GUI:**
   - **Device:** `/dev/ttyUSB0` (or `/dev/ttyACM0`)
   - **Baud rate:** `115200`
   - **Data bits:** `8` | **Stop bits:** `1` | **Parity:** `None`
   - **Flow control:** `None`
   - **Line ending dropdown (bottom right):** Select **`CR/LF`** *(Critical: STM32 parser expects `\r\n`)*
4. **Sending Commands:**
   - Type `GET_ANGLE` and hit Enter.
   - Use the input history or save frequent commands into the CuteCom macro panel.

---

### C. Picocom (Terminal CLI — Zero Install)
`picocom` is already installed on your system:
```bash
picocom -b 115200 /dev/ttyUSB0 --echo --imap lfcrlf
```
* **Flags explained:**
  - `-b 115200`: Sets baud rate.
  - `--echo`: Enables local typing echo (STM32 does not echo raw keystrokes).
  - `--imap lfcrlf`: Maps Enter (`LF`) to `CRLF` so the STM32 line parser triggers.
* **Exit:** Press `Ctrl+A` followed by `Ctrl+X`.

---

### D. Native Python Interactive Shell (with Shortcuts)
Run with zero third-party GUI dependencies:
```bash
python3 -B scripts/windows/monitor_uart.py -p /dev/ttyUSB0 -b 115200
```
**Shortcut Keys:**
- `1` + Enter $\to$ Sends `INFO_REQUEST\r\n`
- `2` + Enter $\to$ Sends `GET_ANGLE\r\n`
- `3` + Enter $\to$ Sends `CALIBRATE_IMU\r\n`
- `4` + Enter $\to$ Sends `SET_FILTER_ALPHA:0.96\r\n`
- `q` + Enter $\to$ Quit
