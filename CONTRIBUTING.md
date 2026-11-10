# Contributing to BTL-KTVXL-N1

Welcome to the **BTL-KTVXL-N1** contributor guide. This project implements a Single-MCU STM32F103C8T6 MPU6050 inclinometer in portable **C99** using the bundled STM32 Standard Peripheral Library (SPL V3.5.0) and CMSIS V1.30 in [`third_party/stm32f10x/`](third_party/stm32f10x/Release_Notes.html), paired with native host CMake/CTest suites and Python stdlib verification tools.

Keep changes focused, preserve hardware/host boundaries, and keep **host CTest**, **ARM compile**, and **physical board** verification claims strictly distinct.

---

## 1. Offline Verification & Evidence Boundary Invariants

- **Offline Host Verification:** [`scripts/agent_test.sh`](scripts/agent_test.sh) and [`scripts/agent_uart.py`](scripts/agent_uart.py) rely strictly on **Python stdlib** and local build tools; no `pip install` packages, network requests, or connected boards are required to run host verification.
- **Keep Evidence Scopes Distinct:**
  - **Host CTest (`scripts/agent_test.sh`):** Exercises shared C99 modules and hardware-stubbed drivers (`#else` branches), exhaustive JEDEC/UART boundary checks (`tests/repro/check_flash_uart_boundaries.py`), and `agent_uart.py --self-test` over local POSIX PTYs. Does **not** verify electrical wiring, I2C/SPI signal timing, or STM32 Flash wear.
  - **ARM Compile (`scripts/agent_test.sh --firmware` or `make firmware`):** Compiles and links Cortex-M3 ELF/BIN/HEX binaries against `third_party/stm32f10x/`. Does **not** flash or validate runtime behavior on silicon.
  - **Physical Board:** Requires real STM32F103C8T6 + MPU6050 + W25Qxx + ILI9341 hardware, `3.3 V` logic/common `GND` checks, ST-Link flashing, and observed UART/TFT output.
- **Safe Linux Serial Access:** Grant UART access through the `dialout` group (`sudo usermod -aG dialout "$USER"`, then log out and back in). **Never** use `chmod 666` on `/dev/ttyUSB*`, `/dev/ttyACM*`, or any `/dev/tty*` node.

---

## 2. Toolchain & Environment Setup

### Ubuntu / Debian

Install the native build, ARM cross-compilation, and optional flashing/serial tools:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake python3 \
  gcc-arm-none-eabi libnewlib-arm-none-eabi # Required for host CTest + ARM firmware build
```

Optional tools for board flashing and interactive UART sessions:

```bash
sudo apt-get install -y --no-install-recommends stlink-tools putty putty-tools # putty-tools provides plink
sudo usermod -aG dialout "$USER" # Grant persistent non-root access to /dev/ttyUSB* and /dev/ttyACM* (re-login required)
```

### Windows 10 / 11

- **Serial Consoles (`PuTTY` / `plink` / PowerShell):**
  - Install PuTTY and `plink.exe` via WinGet (`winget install PuTTY.PuTTY`) or from [chiark PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html).
  - Use [`scripts/windows/launch_putty.bat`](scripts/windows/launch_putty.bat) to auto-detect COM ports and launch PuTTY (`115200 8-N-1`), or use the zero-install PowerShell console [`scripts/windows/serial_terminal.ps1`](scripts/windows/serial_terminal.ps1).
  - [`scripts/windows/monitor_uart.py`](scripts/windows/monitor_uart.py) provides an interactive Python monitor (requires `pyserial`), whereas [`scripts/agent_uart.py`](scripts/agent_uart.py) runs with Python stdlib + `plink.exe` for `--probe-ports` and `--command` (note: `--self-test` requires POSIX PTYs on Linux or WSL).
- **USB-UART & Probe Drivers:** Install the appropriate driver for your hardware adapter (**CH340/CH341**, **Silicon Labs CP2102**, **FTDI FT232**, or **ST-Link VCP/SWD**); see [`docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md`](docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md).
- **ARM Compilation on Windows (Optional):** Install the Arm GNU Toolchain (`arm-none-eabi-gcc`), CMake, and Make to build with [`Makefile`](Makefile) against `third_party/stm32f10x/` without external ST downloads.

---

## 3. Repository Map & Third-Party Policy

| Path | Ownership & Purpose |
|---|---|
| [`src/`](src/) | Production C99 firmware (`main.c`, `src/lib/shared/` portable logic, `src/lib/drivers/` peripheral drivers, `src/lib/stm32/` linker/startup). |
| [`third_party/stm32f10x/`](third_party/stm32f10x/Release_Notes.html) | Bundled STM32F10x SPL V3.5.0 and CMSIS V1.30 used by [`Makefile`](Makefile) for reproducible offline ARM builds. |
| [`tests/`](tests/) | Native CMake/CTest unit, component, and stress runners plus [`tests/repro/check_flash_uart_boundaries.py`](tests/repro/check_flash_uart_boundaries.py). |
| [`scripts/`](scripts/) | Verification and serial tools (`agent_test.sh`, `agent_uart.py`, `monitor.py`, `scripts/linux/`, `scripts/windows/`). |
| [`docs/`](docs/) | Hardware datasheets, [`Wiring_and_Pinouts.md`](docs/Wiring_and_Pinouts.md), [`UART_COMMANDS_REFERENCE.md`](docs/UART_COMMANDS_REFERENCE.md), and OS UART guides. |
| [`.github/`](.github/workflows/ci.yml) | CI workflow, Dependabot config, `CODEOWNERS`, issue forms, and PR template. |

Note: Local directories listed in `.gitignore` (`workspace/`, `team_code/`, `BTLVXL-Nhóm 1/`, `stm32_bringup/`) are personal working areas and must not be required by build scripts, tests, or CI.

### Bundled `third_party/stm32f10x/` Guidelines
- Build ARM firmware against the bundled headers and sources in `third_party/stm32f10x/` (`SPL_DIR ?= third_party/stm32f10x` in [`Makefile`](Makefile)).
- Preserve all STMicroelectronics and ARM CMSIS copyright headers, license notices, and [`Release_Notes.html`](third_party/stm32f10x/Release_Notes.html).
- Do not modify files inside `third_party/stm32f10x/` unless a documented build fix explicitly requires it; explain any third-party delta clearly in the PR.

### Firmware & Coding Conventions
- **Portable C99 & Layering:** Keep hardware-independent math, parsers, and Flash payload codecs in `src/lib/shared/` so CTest verifies the exact production logic natively on the host.
- **Short ISRs:** Keep `TIM2` and `USART1` `RXNE` interrupt handlers minimal (set flags or queue bytes); never perform blocking I2C or SPI Flash waits inside an ISR.
- **Input & Bounds Safety:** Validate UART line terminations (`CR`/`LF`/`CRLF`), filter alpha bounds (`0.50 .. 0.999`), and W25Qxx JEDEC/sector boundaries.
- **Clean Python Execution:** Invoke repository Python tools with `python3 -B` (or `PYTHONDONTWRITEBYTECODE=1`) to avoid generating `__pycache__/` clutter inside the working tree.

---

## 4. Four-Tier Verification Runbook

Run all commands from the repository root (`BTL_KTVXL/`).

### 1. Inspection & Syntax Probes (Read-Only)

```bash
git status --short                                                                        # Verify existing workspace state before editing
cc --version && cmake --version && python3 --version                                      # Confirm native C99 + CMake + Python 3 toolchain
arm-none-eabi-gcc --version                                                               # Required for ARM firmware build
bash -n scripts/*.sh scripts/linux/*.sh                                                   # Check Bash syntax across host/Linux scripts
PYTHONPYCACHEPREFIX="${TMPDIR:-/tmp}/pycache" python3 -B -m py_compile scripts/*.py scripts/linux/*.py scripts/windows/*.py tests/repro/*.py # Bytecode-compile Python scripts without repo __pycache__ writes
```

*Expected Healthy Signature:* All tools report valid versions, `bash -n` and `py_compile` exit `0` with no stdout/stderr errors.

### 2. Setup & Isolated Build (Mutation)

```bash
git switch -c fix/short-description                                                       # Create a scoped branch for your change
make BUILD_DIR=/tmp/btl-firmware firmware                                                 # Compile ARM firmware into /tmp without polluting workspace
```

*Expected Healthy Signature:* `firmware.elf`, `firmware.bin`, `firmware.hex`, and `firmware.map` are generated inside `/tmp/btl-firmware/firmware/` and `arm-none-eabi-size` prints section sizes.

### 3. Verification & Oracle Gate (Proof)

```bash
bash scripts/agent_test.sh                                                                # Offline host CTest (4/4), boundary checks, and agent_uart --self-test
bash scripts/agent_test.sh --firmware                                                     # Full offline gate: adds ARM compile/link (never flashes hardware)
python3 -B scripts/agent_uart.py --self-test                                              # Standalone PTY + mock-plink framing and --allow-write gate check
git diff --check                                                                          # Fail if trailing whitespace or conflict markers were introduced
```

CI ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs `bash -n`, `py_compile`, `bash scripts/agent_test.sh`, and `make BUILD_DIR="$RUNNER_TEMP/firmware-build" firmware`, then packages `firmware.elf`, `firmware.bin`, `firmware.hex`, `firmware.map`, `REVISION.txt`, `TOOLCHAIN.txt`, and `SHA256SUMS` as a 14-day artifact. After downloading and extracting a CI artifact directory, verify integrity from inside that directory:

```bash
sha256sum --check SHA256SUMS                                                              # Every packaged file must report OK
```

*(Note: Passing `SHA256SUMS` verifies archive integrity only; it is neither a cryptographic signature nor proof of physical board testing.)*

### 4. Rollback & Recovery (Safety)

```bash
git revert <your-commit-sha>                                                              # Cleanly undo a committed change while preserving history
make clean                                                                                # Remove local ./build/ artifacts if created
```

If testing firmware on a physical STM32F103C8T6 board, back up your known-good binary before flashing; restore MCU internal Flash with:

```bash
st-flash --reset write /path/to/known-good.bin 0x08000000                                 # Restores internal STM32 Flash only; does NOT revert W25Qxx external Flash
```

---

## 5. Pull Request Standards

When opening a pull request, fill out [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md) completely:

1. **Summary & Scope:** Explain the exact problem and impacted subsystems (`MPU6050`, `W25Qxx`, `ILI9341`, `USART1`, `Calibration/Filter`, `Host tests/CI`, or `Scripts/PuTTY`).
2. **Exact Verification Evidence:** Paste the exact commands executed and observed counts/outcomes while keeping **host CTest**, **ARM compile**, and **physical board** results separate.
3. **Test Integrity:** Do not weaken, skip, delete, or mock away failing assertions to obtain a green pass. Add focused regression tests for behavioral changes.
4. **Documentation Sync:** Update [`README.md`](README.md), [`docs/Wiring_and_Pinouts.md`](docs/Wiring_and_Pinouts.md), [`docs/UART_COMMANDS_REFERENCE.md`](docs/UART_COMMANDS_REFERENCE.md), or platform UART setup docs ([Windows](docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md), [Linux](docs/LINUX_UART_SETUP_HERCULES_EQUIVALENTS.md)) whenever pins, protocols, or scripts change.
5. **Governance & Security:** Follow the [Code of Conduct](CODE_OF_CONDUCT.md) and report vulnerabilities privately per [SECURITY.md](SECURITY.md).
