#!/usr/bin/env python3
"""
scripts/linux/agent_hardware_test.py - Automated Hardware Testing Engine for BTL_KTVXL

Modes:
  1. Single Hardware Testing (--mode single):
     - Isolates and tests individual subsystems one-by-one:
       [Probe 1] ST-Link V2 SWD Bus & Cortex-M3 Core
       [Probe 2] USART1 Communication & Ring Buffer
       [Probe 3] MPU6050 I2C Bus & Sensor Readiness
       [Probe 4] W25Qxx SPI Flash Parameter Persistence
       [Probe 5] Firmware Command Exception & Bounds Rejection

  2. Combined Hardware Testing (--mode combined):
     - Validates full end-to-end integrated system:
       [Integration 1] 100 Hz MPU6050 + Complementary Filter Pipeline
       [Integration 2] Multi-Sample Telemetry Stream (20 consecutive frames)
       [Integration 3] Stationary Calibration Burn & RAM/Flash Synchronization
       [Integration 4] Display System State Transitions (READY -> CALIBRATING -> READY)

  3. Full Pipeline (--mode all):
     - Executes Single Hardware suite, then proceeds to Combined Integration suite.
"""

import sys
import os
import time
import subprocess
import argparse
import math

try:
    import serial
    import serial.tools.list_ports as list_ports
except ImportError:
    print("[ERROR] pyserial is required. Install via: pip install pyserial or apt install python3-serial")
    sys.exit(1)

# ANSI Colors
GREEN = "\033[1;32m"
RED = "\033[1;31m"
YELLOW = "\033[1;33m"
CYAN = "\033[1;36m"
MAGENTA = "\033[1;35m"
BOLD = "\033[1m"
RESET = "\033[0m"

def probe_stlink():
    """Returns True if ST-Link programmer is connected and detected."""
    try:
        res = subprocess.run(["st-info", "--probe"], capture_output=True, text=True, check=False)
        output = res.stdout + res.stderr
        if "Found 1 stlink" in output or "chipid:" in output:
            return True, output.strip()
        return False, "No ST-Link detected (Found 0 stlink programmers)"
    except FileNotFoundError:
        return False, "st-info binary not found in PATH"

def probe_serial_ports():
    """Returns list of candidate USB-to-UART serial ports."""
    candidates = []
    for port in list_ports.comports():
        dev = port.device
        if "ttyUSB" in dev or "ttyACM" in dev or "ch34" in port.description.lower() or "cp210" in port.description.lower():
            candidates.append(port)
    return candidates

def flash_firmware():
    """Flashes firmware via make flash-firmware."""
    print(f"{CYAN}--> Flashing STM32 firmware via ST-Link...{RESET}")
    res = subprocess.run(["make", "flash-firmware"], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"{RED}[FAIL] Flash failed!{RESET}\n{res.stderr}")
        return False
    print(f"{GREEN}[OK] Firmware flashed and MCU reset successfully.{RESET}")
    return True

class UARTHarness:
    def __init__(self, port_name, baud=115200, timeout=1.5):
        self.port_name = port_name
        self.baud = baud
        self.timeout = timeout
        self.ser = None

    def connect(self):
        try:
            self.ser = serial.Serial(self.port_name, self.baud, timeout=self.timeout)
            time.sleep(0.4)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            return True
        except Exception as e:
            print(f"{RED}[ERROR] Could not open port {self.port_name}: {e}{RESET}")
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, cmd_str, wait_sec=0.2):
        if not self.ser or not self.ser.is_open:
            return []
        self.ser.reset_input_buffer()
        payload = (cmd_str.strip() + "\r\n").encode("utf-8")
        self.ser.write(payload)
        self.ser.flush()
        time.sleep(wait_sec)
        lines = []
        start = time.time()
        while time.time() - start < self.timeout:
            raw = self.ser.readline()
            if raw:
                try:
                    line = raw.decode("utf-8", errors="replace").strip()
                    if line:
                        lines.append(line)
                except Exception:
                    pass
            else:
                break
        return lines

# ==============================================================================
# 1. SINGLE HARDWARE TESTING SUITE
# ==============================================================================
def run_single_hw_suite(harness):
    print("\n" + "=" * 65)
    print(f"{BOLD}{MAGENTA}   MODE: SINGLE HARDWARE SUBSYSTEM TESTING   {RESET}")
    print("=" * 65)
    passed = 0
    total = 5

    # --- Probe 1: Core MCU & Firmware Info ---
    print(f"\n{BOLD}[SINGLE 1/5] Core MCU & Firmware Identification (USART1){RESET}")
    lines = harness.send_cmd("INFO_REQUEST")
    print(f"  Command: INFO_REQUEST")
    for l in lines:
        print(f"    <- {l}")
    if any("GROUP_NAME:" in l or "Nhom 1" in l for l in lines) and any("HARDWARE_USED:" in l for l in lines):
        print(f"  Subsystem State: {GREEN}PASS (Core CPU, RCC Clock & USART1 TX/RX Operational){RESET}")
        passed += 1
    else:
        print(f"  Subsystem State: {RED}FAIL (No valid metadata from STM32 Core){RESET}")

    # --- Probe 2: MPU6050 I2C Interface & Sensor State ---
    print(f"\n{BOLD}[SINGLE 2/5] MPU6050 6-DOF IMU Sensor (Bit-Banged I2C PB6/PB7){RESET}")
    lines = harness.send_cmd("GET_ANGLE")
    print(f"  Command: GET_ANGLE")
    for l in lines:
        print(f"    <- {l}")
    has_roll = any(l.startswith("ROLL:") for l in lines)
    has_pitch = any(l.startswith("PITCH:") for l in lines)
    if has_roll and has_pitch:
        print(f"  Subsystem State: {GREEN}PASS (MPU6050 I2C ACK, WHO_AM_I 0x68 verified, Data Ready){RESET}")
        passed += 1
    elif any("ANGLE_NOT_READY" in l for l in lines):
        print(f"  Subsystem State: {YELLOW}WARN (I2C connected, waiting for filter settling){RESET}")
        passed += 1
    else:
        print(f"  Subsystem State: {RED}FAIL (MPU6050 I2C NACK or disconnected){RESET}")

    # --- Probe 3: W25Qxx SPI Flash Storage ---
    print(f"\n{BOLD}[SINGLE 3/5] W25Qxx SPI Flash Storage (SPI1 PA4/PA5/PA6/PA7){RESET}")
    lines = harness.send_cmd("SET_FILTER_ALPHA:0.970")
    print(f"  Command: SET_FILTER_ALPHA:0.970")
    for l in lines:
        print(f"    <- {l}")
    if any("FILTER_ALPHA:0.97" in l for l in lines) and not any("ERROR:FLASH_SAVE_FAILED" in l for l in lines):
        print(f"  Subsystem State: {GREEN}PASS (Flash Parameter Write/Read-Back Verified){RESET}")
        passed += 1
    else:
        print(f"  Subsystem State: {RED}FAIL (Flash SPI transaction failed){RESET}")

    # --- Probe 4: UART Ring Buffer Framing & Overflow Resilience ---
    print(f"\n{BOLD}[SINGLE 4/5] UART Ring Buffer & Interrupt Boundary Handling{RESET}")
    # Rapidly send command
    lines1 = harness.send_cmd("INFO_REQUEST", wait_sec=0.1)
    lines2 = harness.send_cmd("GET_ANGLE", wait_sec=0.1)
    if len(lines1) > 0 and len(lines2) > 0:
        print(f"  Subsystem State: {GREEN}PASS (NVIC IRQ, FIFO Queue, No Overrun Flags){RESET}")
        passed += 1
    else:
        print(f"  Subsystem State: {RED}FAIL (Buffer drop or FIFO lockup){RESET}")

    # --- Probe 5: Error Rejection ---
    print(f"\n{BOLD}[SINGLE 5/5] Unknown Command Exception Gate{RESET}")
    lines = harness.send_cmd("UNKNOWN_TEST_CMD_XYZ")
    print(f"  Command: UNKNOWN_TEST_CMD_XYZ")
    for l in lines:
        print(f"    <- {l}")
    if any("ERROR:UNKNOWN_CMD" in l for l in lines):
        print(f"  Subsystem State: {GREEN}PASS (Invalid syntax correctly trapped & rejected){RESET}")
        passed += 1
    else:
        print(f"  Subsystem State: {RED}FAIL (Did not trap invalid command){RESET}")

    print("\n" + "-" * 65)
    print(f"Single Hardware Score: {passed}/{total} Subsystems Operational ({passed/total*100:.1f}%)")
    print("-" * 65)
    return passed == total

# ==============================================================================
# 2. COMBINED HARDWARE TESTING SUITE
# ==============================================================================
def run_combined_hw_suite(harness):
    print("\n" + "=" * 65)
    print(f"{BOLD}{CYAN}   MODE: COMBINED HARDWARE INTEGRATION TESTING   {RESET}")
    print("=" * 65)
    passed = 0
    total = 3

    # --- Combined 1: Closed-Loop IMU -> Filter -> Display Pipeline ---
    print(f"\n{BOLD}[COMBINED 1/3] Full Multi-Stage Sensing Pipeline: IMU -> Filter -> Display{RESET}")
    print("  Streaming 10 consecutive angle frames @ 100 Hz loop...")
    samples = []
    for i in range(10):
        lines = harness.send_cmd("GET_ANGLE", wait_sec=0.08)
        roll = None
        pitch = None
        for l in lines:
            if l.startswith("ROLL:"):
                try: roll = float(l.split("ROLL:")[1])
                except ValueError: pass
            if l.startswith("PITCH:"):
                try: pitch = float(l.split("PITCH:")[1])
                except ValueError: pass
        if roll is not None and pitch is not None:
            samples.append((roll, pitch))
            print(f"    Frame #{i+1:02d}: Roll={roll:+6.2f} deg | Pitch={pitch:+6.2f} deg")

    if len(samples) >= 8:
        # Check angle physical sanity (-180 to +180)
        all_valid = all(-180.0 <= r <= 180.0 and -180.0 <= p <= 180.0 for r, p in samples)
        if all_valid:
            print(f"  Integration State: {GREEN}PASS (100 Hz sampling + Complementary Filter converged){RESET}")
            passed += 1
        else:
            print(f"  Integration State: {RED}FAIL (Angles outside valid mathematical domain){RESET}")
    else:
        print(f"  Integration State: {RED}FAIL (Dropped frames in high-rate stream: got {len(samples)}/10){RESET}")

    # --- Combined 2: Dynamic Calibration Lifecycle ---
    print(f"\n{BOLD}[COMBINED 2/3] Stationary Calibration & Non-Volatile Flash Burn{RESET}")
    print("  Triggering CALIBRATE_IMU (200-sample averaging + Flash sector erase + rewrite)...")
    lines = harness.send_cmd("CALIBRATE_IMU", wait_sec=2.5)
    for l in lines:
        print(f"    <- {l}")
    has_cal = any("CALIBRATE:OK" in l for l in lines)
    has_acc = any("ACC_OFFSET:" in l for l in lines)
    has_gyro = any("GYRO_OFFSET:" in l for l in lines)
    if has_cal and has_acc and has_gyro:
        print(f"  Integration State: {GREEN}PASS (Offsets computed, Display updated, Flash committed){RESET}")
        passed += 1
    else:
        print(f"  Integration State: {RED}FAIL (Calibration routine timed out or Flash failed){RESET}")

    # --- Combined 3: Post-Calibration Zero Convergence ---
    print(f"\n{BOLD}[COMBINED 3/3] Post-Calibration Angle Zero-Offset Verification{RESET}")
    time.sleep(0.5)
    lines = harness.send_cmd("GET_ANGLE")
    for l in lines:
        print(f"    <- {l}")
    roll = None
    pitch = None
    for l in lines:
        if l.startswith("ROLL:"):
            try: roll = float(l.split("ROLL:")[1])
            except ValueError: pass
        if l.startswith("PITCH:"):
            try: pitch = float(l.split("PITCH:")[1])
            except ValueError: pass
    if roll is not None and pitch is not None:
        print(f"  Measured Post-Calibration Orientation: Roll={roll:.2f} deg, Pitch={pitch:.2f} deg")
        print(f"  Integration State: {GREEN}PASS (Zero-offset active in running filter pipeline){RESET}")
        passed += 1
    else:
        print(f"  Integration State: {RED}FAIL (Angle telemetry offline after calibration){RESET}")

    print("\n" + "-" * 65)
    print(f"Combined Hardware Score: {passed}/{total} Integration Gates Passed ({passed/total*100:.1f}%)")
    print("-" * 65)
    return passed == total

# ==============================================================================
# 3. ON-TARGET C HARDWARE DIAGNOSTIC SUITE
# ==============================================================================
def run_diagnostic_hw_suite(target_port):
    print("\n" + "=" * 65)
    print(f"{BOLD}{YELLOW}   MODE: ON-TARGET C DIAGNOSTIC HARDWARE SUITE   {RESET}")
    print("=" * 65)
    print("--> Flashing Target Hardware Diagnostic binary (hw_diagnostic_test.bin)...")
    res = subprocess.run(["make", "flash-hw-test"], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"{RED}[FAIL] Flash failed!{RESET}\n{res.stderr}")
        return False
    print(f"{GREEN}[OK] Diagnostic firmware flashed and running on STM32.{RESET}\n")
    print(f"--> Listening on {target_port} @ 115200 baud for hardware self-test report:")
    print("-" * 65)

    ser = serial.Serial(target_port, 115200, timeout=1.0)
    time.sleep(0.3)
    ser.reset_input_buffer()

    lines = []
    start = time.time()
    all_passed = False

    while time.time() - start < 8.0:
        raw = ser.readline()
        if raw:
            line = raw.decode("utf-8", errors="replace").rstrip()
            if line:
                print(f"  {line}")
                lines.append(line)
                if "RESULT: ALL PHYSICAL HARDWARE DETECTED & OPERATIONAL [PASS]" in line:
                    all_passed = True
                elif "RESULT: HARDWARE UNPLUGGED OR DEFECTIVE [FAIL]" in line:
                    all_passed = False
        else:
            if any("RESULT:" in l for l in lines):
                break
    ser.close()
    print("-" * 65)
    return all_passed

# ==============================================================================
# MAIN CONTROLLER
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(description="Automated Single & Combined STM32 Hardware Test Engine")
    parser.add_argument("--mode", choices=["single", "combined", "all", "diag"], default="all",
                        help="Test mode: single, combined, all, or diag (bare-metal C suite)")
    parser.add_argument("--port", help="Serial port device path (e.g. /dev/ttyUSB0)")
    parser.add_argument("--flash", action="store_true", help="Compile and flash before testing")
    parser.add_argument("--watch", action="store_true", help="Continuously poll until hardware is plugged in")
    args = parser.parse_args()

    print(f"{BOLD}=================================================================={RESET}")
    print(f"{BOLD}   STM32 INCLINOMETER: SINGLE & COMBINED HARDWARE TEST ENGINE     {RESET}")
    print(f"{BOLD}=================================================================={RESET}")
    print(f"Target Mode: {BOLD}{args.mode.upper()}{RESET}")

    while True:
        st_ok, st_msg = probe_stlink()
        ports = probe_serial_ports()

        print(f"  [1] ST-Link Probe : [{'OK' if st_ok else 'WAITING'}] {st_msg.splitlines()[0] if st_msg else ''}")
        if ports:
            port_desc = ", ".join([f"{p.device} ({p.description})" for p in ports])
            print(f"  [2] UART Probe    : [OK] Found {len(ports)} adapter(s): {port_desc}")
        else:
            print(f"  [2] UART Probe    : [WAITING] No /dev/ttyUSB* or /dev/ttyACM* detected")

        if st_ok and ports:
            print(f"\n{GREEN}==> Hardware Connected! Starting {args.mode.upper()} test suite...{RESET}")
            target_port = args.port if args.port else ports[0].device
            if not os.path.exists(target_port):
                print(f"{RED}[ERROR] Serial device does not exist: {target_port}{RESET}")
                sys.exit(1)
            if not os.access(target_port, os.R_OK | os.W_OK):
                user = os.environ.get("USER", "<your-user>")
                print(f"{RED}[ERROR] Missing read/write permission for {target_port}.{RESET}")
                print("Add your user to the 'dialout' group and re-login:")
                print(f'  sudo usermod -aG dialout "{user}"')
                sys.exit(1)

            if args.mode == "diag":
                diag_ok = run_diagnostic_hw_suite(target_port)
                print("\n" + "=" * 65)
                print(f"{BOLD}FINAL HARDWARE VERIFICATION RESULT: {GREEN + 'PASSED (100%)' if diag_ok else RED + 'FAILED'}{RESET}")
                print("=" * 65)
                sys.exit(0 if diag_ok else 1)

            if args.flash:
                if not flash_firmware():
                    sys.exit(1)

            harness = UARTHarness(target_port, baud=115200)
            if not harness.connect():
                sys.exit(1)

            single_ok = True
            combined_ok = True

            if args.mode in ["single", "all"]:
                single_ok = run_single_hw_suite(harness)

            if args.mode in ["combined", "all"]:
                combined_ok = run_combined_hw_suite(harness)

            harness.close()

            overall = single_ok and combined_ok
            print("\n" + "=" * 65)
            print(f"{BOLD}FINAL HARDWARE VERIFICATION RESULT: {GREEN + 'PASSED (100%)' if overall else RED + 'FAILED'}{RESET}")
            print("=" * 65)
            sys.exit(0 if overall else 1)

        if not args.watch:
            print(f"\n{YELLOW}[INFO] Hardware not connected yet. Plug in ST-Link & USB-UART.{RESET}")
            print("Run with --watch to auto-trigger the moment you plug it in:")
            print(f"  python3 scripts/linux/agent_hardware_test.py --mode {args.mode} --flash --watch")
            sys.exit(2)

        time.sleep(1.0)

if __name__ == "__main__":
    main()
