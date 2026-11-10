# Firmware Bring-Up and Troubleshooting Runbook

**Project:** BTL-KTVXL-N1 tilt measurement system  
**Date:** 2026-10-10  
**Boards:** STM32F103C8T6 Blue Pill Type-C + ESP32-WROOM-32 CH340 30-pin Type-C + ILI9341 2.8" SPI TFT + MPU6050 + W25Q64  

This runbook records the exact firmware, flashing, wiring, and troubleshooting state reached during the 2026-10-10 bring-up session.

---

## 1. Current Known-Good State

### 1.1 ESP32 + ILI9341 Display

The ESP32 display path is confirmed working.

Evidence observed during the session:

- ESP32 uploads successfully over `/dev/ttyUSB0` through CH340.
- ESP32 boot log appeared in serial monitor:

```text
=== ESP32 ILI9341 TILT MONITOR INITIALIZING ===
[UART] Serial2 initialized on RX=16, TX=17 @ 115200
[TFT] ILI9341 Dashboard Ready.
```

- TFT initially stayed white until the backlight/control state was corrected.
- The old minimal `src/TftTest.ino` was uploaded as an A/B diagnostic and proved the TFT could render.
- The final dashboard firmware was restored and the dashboard became available.

Current ESP32 firmware path:

```text
src/esp32_firmware/esp32_firmware.ino
```

Important ESP32 TFT pins in firmware:

```cpp
#define TFT_CS        15
#define TFT_DC        2
#define TFT_RST       4
#define TFT_BL_GPIO5  5
#define TFT_BL_GPIO32 32
```

The firmware drives both `GPIO5` and `GPIO32` HIGH at boot as a temporary backlight compatibility probe.

```text
ponytail: both backlight GPIOs are driven because the physical LED/BLK jumper was uncertain during bring-up. Delete the unused one after confirming the actual wire.
```

### 1.2 STM32 Bring-Up Firmware

The STM32 is detected by ST-LINK:

```text
Found 1 stlink programmers
version:    V2J37S7
flash:      131072
sram:       20480
chipid:     0x410
dev-type:   STM32F1xx_MD
```

A minimal STM32F103 bare-metal UART bring-up firmware was added and flashed.

Current bring-up source:

```text
stm32_bringup/main.c
stm32_bringup/startup_stm32f103c8tx.s
stm32_bringup/STM32F103C8TX_FLASH.ld
```

Build artifact:

```text
build/stm32_bringup.bin
```

Flash command:

```bash
make flash-stm32-bringup
```

Flash evidence:

```text
Flash written and verified! jolly good!
```

This STM32 bring-up firmware is **not** the full MPU6050/W25Q64 firmware yet. It sends simulated binary telemetry frames over USART1 TX on `PA9` so the ESP32 dashboard can prove the UART path.

---

## 2. Current Physical Wiring Expectations

### 2.1 ESP32 to ILI9341 TFT

| TFT Pin | ESP32 Pin | Purpose |
|---|---|---|
| `VCC` | `3.3V` or `5V/VIN` | TFT module power. Use 5V only if module has onboard regulator. |
| `GND` | `GND` | Common ground. |
| `CS` | `GPIO15 / D15` | SPI chip select. |
| `RESET / RST` | `GPIO4 / D4` | TFT reset. |
| `DC / RS` | `GPIO2 / D2` | Data/command select. |
| `SDI / MOSI` | `GPIO23 / D23` | SPI MOSI. |
| `SCK / CLK` | `GPIO18 / D18` | SPI clock. |
| `LED / BLK` | `3.3V`, `GPIO5`, or `GPIO32` | Backlight. Current firmware drives GPIO5 and GPIO32 HIGH. |
| `SDO / MISO` | `GPIO19 / D19` or NC | Optional; not needed for drawing. |

### 2.2 STM32 to ESP32 UART

| STM32 Blue Pill | ESP32 DevKit | Purpose |
|---|---|---|
| `A9 / PA9 / USART1_TX` | `GPIO16 / RX2` | STM32 telemetry to ESP32. Required for bring-up. |
| `A10 / PA10 / USART1_RX` | `GPIO17 / TX2` | ESP32 commands to STM32. Required for full firmware later. |
| `GND` | `GND` | Shared ground. Required. |

For the current STM32 bring-up firmware, only this telemetry wire is strictly required:

```text
STM32 A9 / PA9 -> ESP32 GPIO16 / RX2
```

### 2.3 STM32 to MPU6050

| MPU6050 | STM32 |
|---|---|
| `VCC` | `3.3V` rail |
| `GND` | Common `GND` rail |
| `SCL` | `B6 / PB6` |
| `SDA` | `B7 / PB7` |
| `AD0` | `GND` for address `0x68` |
| `INT` | `B5 / PB5` optional |

### 2.4 STM32 to W25Q64

| W25Q64 | STM32 |
|---|---|
| `VCC` | `3.3V` only |
| `GND` | Common `GND` rail |
| `CS` | `A4 / PA4` |
| `CLK` | `A5 / PA5` |
| `DO / MISO` | `A6 / PA6` |
| `DI / MOSI` | `A7 / PA7` |

Never connect W25Q64 VCC to 5V.

---

## 3. Toolchain and Installed Tools

Checked or installed during session:

```text
arm-none-eabi-gcc       present
arm-none-eabi-objcopy   present
gdb-multiarch           present
st-flash                present
st-info                 present
openocd                 present
dfu-util                present
stm32flash              installed during session
arduino-cli             present
```

Check tools:

```bash
for t in arm-none-eabi-gcc arm-none-eabi-objcopy gdb-multiarch st-flash st-info openocd stm32flash dfu-util arduino-cli; do
  printf '%-24s' "$t"
  command -v "$t" || true
done
```

---

## 4. Build and Flash Commands

Run all commands from repo root:

```bash
cd /home/vgbn/Documents/codeptit/stm32-esp32/BTL_KTVXL
```

### 4.1 Run Host Unit Tests

```bash
make test
```

Expected:

```text
=== ALL FIRMWARE TESTS PASSED (100% SUCCESS) ===
```

### 4.2 Build STM32 Bring-Up Firmware

```bash
make stm32-bringup
```

Output:

```text
build/stm32_bringup.elf
build/stm32_bringup.bin
```

### 4.3 Flash STM32 Bring-Up Firmware

```bash
make flash-stm32-bringup
```

Expected:

```text
Flash written and verified! jolly good!
```

### 4.4 Compile ESP32 Dashboard Firmware

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 src/esp32_firmware
```

### 4.5 Upload ESP32 Dashboard Firmware

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 src/esp32_firmware
```

If the port changes, list boards:

```bash
arduino-cli board list
```

### 4.6 Open ESP32 Serial Monitor

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200 -c dtr=off -c rts=off
```

Normal ESP32 boot log:

```text
=== ESP32 ILI9341 TILT MONITOR INITIALIZING ===
[UART] Serial2 initialized on RX=16, TX=17 @ 115200
[TFT] ILI9341 Dashboard Ready.
```

---

## 5. Troubleshooting Log and Root Causes

### 5.1 ESP32 Monitor Initially Blank

Symptom:

```text
arduino-cli monitor connected but printed nothing.
```

Resolution:

- Pressing/resetting the ESP32 produced boot logs.
- Use monitor with DTR/RTS disabled during troubleshooting:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200 -c dtr=off -c rts=off
```

### 5.2 TFT Initially Blank / Backlight Off

Symptom:

```text
ESP32 firmware booted but TFT looked dead.
```

Root cause:

```text
TFT LED/BLK backlight was likely connected to a GPIO not driven by firmware.
```

Fix:

Current firmware drives both possible documented backlight pins HIGH:

```cpp
pinMode(TFT_BL_GPIO5, OUTPUT);
digitalWrite(TFT_BL_GPIO5, HIGH);
pinMode(TFT_BL_GPIO32, OUTPUT);
digitalWrite(TFT_BL_GPIO32, HIGH);
```

Future cleanup:

- Identify which physical jumper is actually used for `LED/BLK`.
- Keep only that GPIO, or wire `LED/BLK` directly to `3.3V` for always-on backlight.

### 5.3 TFT White Screen

Symptom:

```text
Backlight was on, but TFT was all white.
```

Bayesian isolation performed:

- Uploaded temporary diagnostic sketch cycling red/green/blue/black.
- Uploaded old known-good `src/TftTest.ino` as A/B reference.
- Dashboard eventually became available again after reset/boot state handling.

Interpretation:

- TFT hardware and SPI wiring are now known-good because the dashboard renders.
- If white screen reappears, first suspect reset/boot state or loose control wires.

Most important wires to check if white screen returns:

```text
TFT CS        -> ESP32 GPIO15 / D15
TFT RST       -> ESP32 GPIO4  / D4
TFT DC / RS   -> ESP32 GPIO2  / D2
TFT SDI/MOSI  -> ESP32 GPIO23 / D23
TFT SCK/CLK   -> ESP32 GPIO18 / D18
```

### 5.4 STM32 Build Initially Failed

Command:

```bash
make stm32-bringup
```

Initial errors:

```text
undefined reference to `_sidata`
undefined reference to `memset`
```

Root cause:

- Linker script missed `_sidata = LOADADDR(.data);`.
- Freestanding STM32 build used `memset` without libc.

Fix:

- Added `_sidata` to linker script.
- Added tiny local `memset()` implementation in `stm32_bringup/main.c`.

---

## 6. Current Firmware Roles

### ESP32

Runs the final dashboard firmware:

```text
src/esp32_firmware/esp32_firmware.ino
```

Responsibilities:

- Initialize TFT.
- Drive backlight probe pins.
- Read STM32 binary telemetry on UART2 RX (`GPIO16`).
- Render pitch/roll/yaw dashboard.
- Bridge USB serial commands to STM32 on UART2 TX (`GPIO17`).

### STM32

Currently runs minimal bring-up firmware:

```text
stm32_bringup/main.c
```

Responsibilities:

- Configure USART1 TX on `PA9`.
- Blink `PC13`.
- Send simulated binary telemetry frames to ESP32.

Not yet active in STM32 bring-up firmware:

```text
MPU6050 I2C reads
W25Q64 SPI config save/load
Complementary filter from real sensor data
Command parser on PA10 RX
```

These exist in source modules, but need full STM32 HAL/Cube or bare-metal integration before flashing as the final production STM32 firmware.

---

## 7. Next Session Plan

1. **Prove STM32 UART telemetry on dashboard**
   - Confirm dashboard values change after flashing `stm32_bringup.bin`.
   - If values do not move, check `STM32 PA9 -> ESP32 GPIO16` and shared GND.

2. **Promote STM32 from bring-up to real sensor firmware**
   - Either generate a STM32CubeIDE project or extend the bare-metal bring-up firmware.
   - Add I2C1 MPU6050 init/read.
   - Add Complementary Filter update loop.
   - Add USART1 binary telemetry using real pitch/roll.

3. **Then add W25Q64 config storage**
   - Flash config sector read/write.
   - Save calibration offsets and filter alpha.

4. **Final cleanup after hardware certainty**
   - Remove unused backlight GPIO (`GPIO5` or `GPIO32`).
   - Update `docs/Wiring_and_Pinouts.md` with the actual confirmed LED/BLK wiring.

---

## 8. Quick Next Commands

```bash
cd /home/vgbn/Documents/codeptit/stm32-esp32/BTL_KTVXL
make test
make stm32-bringup
make flash-stm32-bringup
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 src/esp32_firmware
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200 -c dtr=off -c rts=off
```
