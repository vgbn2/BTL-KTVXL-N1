# Security Policy

## Supported Versions

Security fixes target the current default branch (`main`). Older commits and downloaded firmware artifacts do not have a separate maintenance commitment.

| Version | Supported |
|---|---|
| `main` (latest) | ✅ |
| Older commits / archived build artifacts | ❌ |

## Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Use [GitHub private vulnerability reporting](https://github.com/vgbn2/BTL-KTVXL-N1/security/advisories/new) to submit a report confidentially. If private vulnerability reporting is disabled on the repository, open a short public issue asking the maintainer to enable it without posting any exploit details, credentials, or private host data.

Include in your report:
- Affected commit SHA and component (`MPU6050`, `W25Qxx`, `ILI9341`, `USART1`, `Calibration/Filter`, `Host tests/CI`, or `Scripts/PuTTY`).
- Expected behavior, observed behavior, and potential impact.
- Minimal reproduction steps or redacted transcripts.
- For hardware findings: MCU/board revision, wiring and voltage levels, UART input sent, and whether the issue was reproduced on a physical board or only in a host test.
- Redact personal details, local usernames, host paths, and unrelated device identifiers from logs.

### Response expectations

This is a university course and maintainer-led embedded repository. Reports are handled on a best-effort basis with no SLA or response-time guarantee implied:

| Stage | Best-effort target |
|---|---|
| Initial acknowledgement | Within 5 business days |
| Triage and severity assessment | Within 14 business days |
| Fix or mitigation guidance | Depends on severity and hardware verification access |

## Scope

In scope for security and safety review:
- Firmware memory safety and bounds validation in [`src/`](src/) (`USART1` command parsing in [`src/lib/shared/uart_cmd.c`](src/lib/shared/uart_cmd.c), Flash config handling in [`src/lib/drivers/flash_config.c`](src/lib/drivers/flash_config.c), and sensor/display drivers).
- Host-side scripts in [`scripts/`](scripts/) that handle serial device input, subprocess arguments, or local files.
- Build dependency integrity (`third_party/stm32f10x/`) and least-privilege GitHub Actions configuration ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)).

Out of scope / verification boundaries:
- Native host CMake/CTest suites use stubbed hardware interfaces and do not prove electrical safety, bus signal integrity, real-time deadline compliance, or fitness for safety-critical use.
- CI compiles ARM firmware artifacts in an isolated runner without connecting to, flashing, or testing physical STM32 hardware.
- Local ignored working directories (`.gitignore` entries such as `workspace/`, `team_code/`, `BTLVXL-Nhóm 1/`, `stm32_bringup/`) are outside supported CI deliverables.

## Serial, Hardware, and Host Safety Notes

- **Linux serial permissions:** Grant serial port access via group membership (`sudo usermod -aG dialout "$USER"` followed by logout/login). **Never** use `chmod 666` on `/dev/ttyUSB*`, `/dev/ttyACM*`, or any `/dev/tty*` device node.
- **Read-only agent serial default:** [`scripts/agent_uart.py`](scripts/agent_uart.py) permits only read-only commands (`INFO_REQUEST`, `INFO_REQUEST:GROUP`, `GET_ANGLE`) by default and blocks Flash- or state-mutating commands (`CALIBRATE_IMU`, `SET_FILTER_ALPHA:...`, or custom strings) unless `--allow-write` is explicitly supplied.
- **Electrical safety:** Verify common `GND` and `3.3 V` logic levels on `PA9`/`PA10`, I2C, SPI, and SWD connections before powering the board.
- **CI artifact verification:** Verify downloaded CI build archives using `sha256sum --check SHA256SUMS` to detect truncated or tampered downloads; SHA-256 checksums verify file integrity only and do not serve as a cryptographic signature or hardware test pass.
