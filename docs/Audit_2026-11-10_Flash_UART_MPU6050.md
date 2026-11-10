# Recent firmware review — 2026-11-10

> **Follow-up implementation:** The user subsequently authorized synchronization. Both outer USER and `src/` now contain the new JEDEC and CR/LF conditions. Full root ARM firmware compile/link and BIN/HEX generation passed; outer USER changed-file ARM compilation passed. The findings below describe the pre-fix audit snapshot.

Audit started: **2026-11-10T06:08:41Z**. Mode: `review`, Hard Reading.

## Scope and revision

Objective: verify the stated Flash/UART changes and compare Trần Đức Anh's MPU6050 code with the exact user-supplied Google Docs tab. Acceptance: preserve the old JEDEC set, add only `20 40 16`, accept CR/LF/CRLF without duplicate commands, establish build ownership and source-level reference parity. Verification: file comparisons, compiled production-source slices, ARM compilation, existing CTest suite, and independently delegated document comparison. No production edits or physical flashing were performed; Windows `D:\TEST_FLASH` was unavailable.

- **N (new):** `BTLVXL-Nhóm 1/BTLVXL-Nhóm 1/USER/`, selected by its sibling `MDK/BTLVXL-Nhóm 1.uvprojx`.
- **O (older):** `BTLVXL-Nhóm 1/USER/`, selected by `BTLVXL-Nhóm 1/MDK/Calibration_An.uvprojx`; `team_code` resolves to this outer project.
- **S:** `src/`, selected by root `make firmware`.
- N versus O: only `flash_config.c` and `uart_cmd.c` change executable tokens. `imu_types.h`, `system_time.c`, and `system_time.h` differ in comments/formatting. Calibration and filter files are identical.

## Confirmed findings

| ID | Severity | Boundary and evidence | Owner / attribution | Clearing gate |
|---|---|---|---|---|
| BTL-01 | High | Root firmware source selection (`Makefile:136-144,179,186`) still uses `src/lib/drivers/flash_config.c:210-213`, rejecting 204016, and `src/lib/shared/uart_cmd.c:258-259`, ignoring CR. N has both fixes at `flash_config.c:129-133`, `uart_cmd.c:223`. Actual source-slice A/B probe: N exit 0; S exit 1. | Firmware integration + Flash/UART; `our_source`; stub `none`; high confidence. | Supported build paths must select the approved predicates/framing, then actual firmware rebuild and save/reset/load bench check. |
| BTL-02 | Medium | `CMakeLists.txt:23-30` and `Makefile:10-12,30-33` omit production `flash_config.c` and `uart_cmd.c` from tests. Fresh suite passes 4/4 while S retains both defects. Generated compile database confirms both missing (coverage probe exit 1). | Firmware test/build integration; `our_source`; simulation stubs are `test_stub_only`; high confidence. | Exercise actual production policy/parser with hardware-boundary fixtures; retain separate real-hardware persistence acceptance. |

Tracking: [BTL-01, issue #2](https://github.com/vgbn2/BTL-KTVXL-N1/issues/2), [BTL-02, issue #1](https://github.com/vgbn2/BTL-KTVXL-N1/issues/1).

These are integration/coverage gaps, not defects introduced by the new JEDEC expression. No failure was reproduced in that expression or the updated receive framing.

## New-code verification

- Compiled the actual JEDEC predicate extracted from N; exhausted **16,777,216** manufacturer/type/capacity combinations: **zero mismatches** against old EF40/EF60 capacities 14–18 plus exact 204016. Explicit SPI-error checks reject all supported IDs.
- Compiled N's actual `USART1_IRQHandler` with a host register fixture: CR, LF, CRLF each enqueue one command; blank delimiters enqueue none; queue saturation and oversized-line recovery checks pass. This is source-slice verification, not electrical UART qualification.
- S A/B: one ID-set mismatch (`204016 -> -4`), CR fails, LF/CRLF pass. The extracted S guard also omits an explicit `spi_error` check; this is not reported as an independently reachable hardware failure.
- Both N files compile with `arm-none-eabi-gcc`, Cortex-M3/thumb/C99, installed SPL/CMSIS headers, `STM32F10X_MD`, and `USE_STDPERIPH_DRIVER`: exit 0, no diagnostics. This is object compilation, not a complete Keil/link/flash proof.
- Fresh out-of-tree CMake build and CTest: **4/4 passed**. Existing tests cover their selected legacy modules, not the two new files.
- `anti-cheat-enforcer --diff --require-tests`: exit 0, zero strikes; limited to the tool's diff scope in this heavily dirty/untracked worktree.
- Complexity tool: exit 1, 24 compressed-statement hotspots across N Flash/UART; pre-existing formatting dominates. No runtime defect inferred from its score. Function classifier: exit 0 but all 20 classifications ambiguous, so no architectural correctness claim.
- No incoming PRs or `origin/repair/*` branches at audit time.

Allowlisting a JEDEC response only allows initialization to continue. Saving still requires write-enable, erase/program, status polling, and matching readback (`N/flash_config.c:137-158`); boot restore also validates magic, CRC and parameter limits (`160-170`). No real-chip save/reset/load or power-interruption test was performed. The single erased configuration sector is not an atomic two-copy persistence design.

## Trần Đức Anh — exact linked reference

[User-supplied tab](https://docs.google.com/document/d/1aQ-MSQ36U3cBjjND7cYixT5sNyXzfwoGBwEQ__545Qs/edit?tab=t.oze5iyx9t8xm).
Public export succeeded. Google Docs metadata maps `t.x1dn4a16fnwq` to **Trần Đức Anh** and its child `t.oze5iyx9t8xm` to **mpu6050.c**. This is a code reference, not a separate prose assignment rubric. Export: 335 lines, UTF-8/BOM-stripped SHA-256 `d6eeb179a747700ca578a04b02488b8dd73a4868437f75b6226c76d21e83a1d3`.

| Reference behavior | N source evidence | Result |
|---|---|---|
| PB6 SCL, PB7 SDA, open-drain | `mpu6050.c:5-6,105-110` | Match |
| Bounded SCL wait, ACK/NACK, repeated START, STOP | `mpu6050.c:12-20,37-93` | Match |
| WHO_AM_I 0x68, reset then gyro-X PLL | `mpu6050.c:95-99,114-132` | Match |
| `reg_write(0x1A, 0x03)`, divider `0x09` | `mpu6050.c:135,138` | Match: configured 100 Hz, not bench-measured |
| ±2g / ±250 dps configuration | `mpu6050.c:141,144` | Match |
| 14-byte burst at 0x3B, null/error checks | `mpu6050.c:154-162` | Match |
| Signed conversion, /16384 and /131 scaling, skip temperature | `mpu6050.c:147-152,163-168` | Match |
| Lowercase `mpu6050_init`, `mpu6050_read_all`, `mpu6050_calibrate_offsets` helpers | Absent from N; present in `src/lib/drivers/mpu6050.c` | Full-file parity partial; not required by retained uppercase firmware callers |
| Host/PC conditional simulation branch | Absent from N; present with extensions in S | Firmware-only N differs intentionally in scope |

All **13 hardware function bodies** match the reference after removing comments and whitespace: `sda`, `scl_low`, `scl_high`, `stop`, `start`, `send_byte`, `receive_byte`, `reg_write`, `reg_read`, `MPU6050_CheckConnection`, `MPU6050_Init`, `bytes_to_i16`, `MPU6050_Read`. O and N `mpu6050.c` are byte-identical. Matching reference code does not independently prove the reference's correctness or sensor accuracy.

## Verification runbook

### 1. Inspection / probe
```sh
git status --short # identify pre-existing edits before any follow-up
```

### 2. Remediation
Audit only. First follow-up: choose the authoritative build path and address BTL-01 before flashing from root targets.

### 3. Verification
```sh
cmake -S . -B /tmp/opencode/btl-audit-build # keep generated build files outside the repository
cmake --build /tmp/opencode/btl-audit-build -j 4 # bounded parallel compilation
ctest --test-dir /tmp/opencode/btl-audit-build --output-on-failure # expose failures; covers existing targets only
anti-cheat-enforcer --diff --require-tests # diff-integrity gate, not hardware proof
```
Session source-slice harness: `/tmp/opencode/btl_changed_boundaries.py`; new/source runs return 0/1 respectively. Temporary evidence is not a permanent regression suite.

### 4. Rollback
No firmware rollback required: production files and device contents were not changed by this audit.

## Authorized follow-up: Flash/UART synchronization

Root cause: the updated nested project was not the source selected by the outer builds. Copied only the JEDEC allowlist/error guard and CR/LF delimiter change to the two outer implementations. Calibration/filter algorithms were not edited.

- Regression: `tests/repro/check_flash_uart_boundaries.py` compiles actual source predicate/ISR slices. Run with `src`, `outer`, or `new`; all three pass. Baseline `src` exited 1 before the fix; the same oracle exits 0 after it. This narrow fixture is not a physical SPI/UART test or full production-module integration test.
- Full root firmware compiled and linked using a temporary copy of the Makefile with only `build/` output paths redirected to `/tmp/opencode/btl-sync-build/`. ELF/BIN/HEX produced; text 34,812 bytes, data 468, BSS 1,044. Newlib/nosys emitted unimplemented syscall warnings; build exit 0.
- Changed outer USER files compiled to ARM objects with no diagnostics. Windows Keil itself was not executed.
- Existing CTest 4/4 and anti-cheat zero strikes. Coverage issue #1 remains open: the new focused check is standalone and existing CMake targets still omit these hardware paths.
- Runnable verification: `python3 -B tests/repro/check_flash_uart_boundaries.py src` (`-B` prevents bytecode writes); repeat with `outer` or `new`. Normal `make firmware` regenerates the standard root output before a later flash operation.
- Rollback: revert only the four Flash/UART condition hunks from this follow-up; do not reset the dirty worktree. No device flashing occurred.
