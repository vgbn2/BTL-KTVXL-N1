## Summary

Describe the problem, affected firmware or host modules, and why this bounded change is needed.

## Related Issues

<!-- Link relevant issues: Closes #123, Fixes #456, Relates to #789 -->
- Closes #
- Relates to #

## Affected Areas

- [ ] `MPU6050` / I2C sensor acquisition
- [ ] `W25Qxx` / SPI Flash configuration storage
- [ ] `ILI9341` / SPI2 display & TIM2 timing
- [ ] `USART1` / UART command parser & telemetry framing
- [ ] `Calibration/Filter` / static offset & complementary filter
- [ ] `Host tests/CI` / CMake, CTest, boundary checks, GitHub Actions
- [ ] `Scripts/PuTTY` / serial launchers, UART CLI, Windows & Linux helpers
- [ ] Documentation / governance / third-party notices

## Verification Evidence

List exact commands executed and their observed outcomes. Keep **host CTest**, **ARM compile**, and **physical board** results strictly distinct, and state any scope not run.

```text
command:
outcome:
evidence scope: [host CTest | ARM compile | physical board]
not run & reason:
```

### Test Integrity & Documentation Checklist
- [ ] Behavior changes include focused regression tests (or this PR explains why a test is not applicable).
- [ ] Host-test, ARM-build, and physical-board claims are separated without conflation.
- [ ] No existing test or assertion was weakened, skipped, mocked away, or suppressed to obtain a pass.
- [ ] Relevant documentation (`README.md`, `docs/Wiring_and_Pinouts.md`, `docs/UART_COMMANDS_REFERENCE.md`, `CONTRIBUTING.md`) is updated for any changed pins, UART commands, storage formats, or scripts.

## Safety and Hardware Boundaries

- [ ] This PR preserves CI least privilege (`permissions: contents: read`), read-only serial defaults, and safe Linux permissions (`dialout` group; never `chmod 666`).
- [ ] Any Flash, ISR, I2C/SPI timing, or wiring/voltage assumption changes are explicitly documented above.

## Compatibility and Rollback

Describe any pinout, UART protocol, or W25Qxx Flash layout compatibility changes and how to revert:
- [ ] Code rollback is clean via `git revert <commit-sha>` (and `make clean` for local artifacts).
- [ ] Hardware / Flash compatibility notes: <!-- e.g. backward-compatible W25Qxx record layout, or steps to re-flash known-good .bin at 0x08000000 -->

## Contribution Attestation

- [ ] I have the right to contribute this material and preserved existing third-party copyright and license notices (including `third_party/stm32f10x/`).
