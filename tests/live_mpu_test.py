#!/usr/bin/env python3
"""
Live MPU6050 Telemetry & Sensor Output Monitor
===============================================
Connects to STM32 / USB-TTL serial stream (default: 115200 baud),
parses both 15-byte binary telemetry frames and human-readable ASCII,
and renders an interactive live gauge in the terminal.

Usage:
    # Test right now without hardware (simulated live stream):
    python3 tests/live_mpu_test.py --sim

    # Live hardware test (auto-detects /dev/ttyUSB* or /dev/ttyACM*):
    python3 tests/live_mpu_test.py

    # Explicit port specification:
    python3 tests/live_mpu_test.py --port /dev/ttyUSB0 --baud 115200
"""

import sys
import time
import glob
import math
import argparse

try:
    import serial
except ImportError:
    serial = None

TELEMETRY_MAGIC_0 = 0xAA
TELEMETRY_MAGIC_1 = 0x55
TELEMETRY_FRAME_LEN = 15

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def render_gauge(val_deg: float, span_deg: float = 90.0, width: int = 24) -> str:
    """Render an ASCII center-zero gauge [-span ... 0 ... +span]."""
    clamped = max(-span_deg, min(span_deg, val_deg))
    ratio = clamped / span_deg  # -1.0 to +1.0
    half = width // 2
    pos = int(round(half + (ratio * half)))
    pos = max(0, min(width, pos))

    bar = list(" " * (width + 1))
    bar[half] = "|"  # Center mark

    if pos > half:
        for i in range(half + 1, pos + 1):
            bar[i] = "="
    elif pos < half:
        for i in range(pos, half):
            bar[i] = "="
    bar[pos] = "#"

    return "".join(bar)

def parse_telemetry_frame(buf: bytes):
    if len(buf) < TELEMETRY_FRAME_LEN:
        return None
    if buf[0] != TELEMETRY_MAGIC_0 or buf[1] != TELEMETRY_MAGIC_1:
        return None
    if buf[2] != TELEMETRY_FRAME_LEN:
        return None

    expected_crc = crc16_ccitt(buf[:13])
    actual_crc = buf[13] | (buf[14] << 8)
    if expected_crc != actual_crc:
        return None

    seq = buf[3]
    type_id = buf[4]
    pitch_raw = int.from_bytes(buf[5:7], byteorder='little', signed=True)
    roll_raw = int.from_bytes(buf[7:9], byteorder='little', signed=True)
    yaw_raw = int.from_bytes(buf[9:11], byteorder='little', signed=True)
    status = int.from_bytes(buf[11:13], byteorder='little', signed=False)

    return {
        "seq": seq,
        "type": type_id,
        "pitch": pitch_raw / 100.0,
        "roll": roll_raw / 100.0,
        "yaw": yaw_raw / 100.0,
        "status": status,
        "crc_ok": True
    }

def auto_detect_serial_port() -> str:
    candidates = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    if candidates:
        return sorted(candidates)[0]
    return ""

def run_simulation():
    print("\n========================================================")
    print("  MPU6050 LIVE TELEMETRY MONITOR (SIMULATION MODE)      ")
    print("  Press Ctrl+C to stop simulation                       ")
    print("========================================================\n")

    seq = 0
    start_time = time.time()
    try:
        while True:
            t = time.time() - start_time
            # Simulate real physical sensor tilt oscillation
            sim_pitch = 35.0 * math.sin(t * 1.5)
            sim_roll  = 20.0 * math.cos(t * 1.2)
            sim_yaw   = (t * 5.0) % 360.0

            pitch_bar = render_gauge(sim_pitch, span_deg=90.0)
            roll_bar  = render_gauge(sim_roll, span_deg=90.0)

            sys.stdout.write(
                f"\r[FRAME #{seq:05d}] "
                f"PITCH: [{pitch_bar}] {sim_pitch:+06.2f}° | "
                f"ROLL: [{roll_bar}] {sim_roll:+06.2f}° | "
                f"STATUS: 0x0003 (OK)"
            )
            sys.stdout.flush()
            seq = (seq + 1) & 0xFF
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\n\n[INFO] Simulation stopped by user.")

def run_live(port: str, baud: int):
    if serial is None:
        print("[ERROR] pyserial is not installed. Install via: pip install pyserial")
        sys.exit(1)

    print(f"\n========================================================")
    print(f"  CONNECTING TO MPU6050 STREAM: {port} @ {baud} BAUD     ")
    print(f"  Press Ctrl+C to exit                                   ")
    print(f"========================================================\n")

    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.1)
    except Exception as e:
        print(f"[ERROR] Failed to open port {port}: {e}")
        sys.exit(1)

    rx_buf = bytearray()
    frame_count = 0
    crc_err_count = 0

    try:
        while True:
            chunk = ser.read(64)
            if chunk:
                rx_buf.extend(chunk)

            # 1. Search for binary telemetry frame [0xAA, 0x55]
            while len(rx_buf) >= TELEMETRY_FRAME_LEN:
                # Find magic header
                if rx_buf[0] != TELEMETRY_MAGIC_0 or rx_buf[1] != TELEMETRY_MAGIC_1:
                    # Check if human-readable ASCII line
                    if b'\n' in rx_buf:
                        idx = rx_buf.index(b'\n')
                        line = rx_buf[:idx].decode('utf-8', errors='ignore').strip()
                        rx_buf = rx_buf[idx+1:]
                        if line:
                            print(f"\n[DEVICE_LOG] {line}")
                        continue
                    else:
                        rx_buf.pop(0)
                        continue

                # Potential frame found
                frame_bytes = bytes(rx_buf[:TELEMETRY_FRAME_LEN])
                frame = parse_telemetry_frame(frame_bytes)
                if frame:
                    rx_buf = rx_buf[TELEMETRY_FRAME_LEN:]
                    frame_count += 1
                    pitch_bar = render_gauge(frame["pitch"], span_deg=90.0)
                    roll_bar  = render_gauge(frame["roll"], span_deg=90.0)

                    sys.stdout.write(
                        f"\r[SEQ #{frame['seq']:03d}] "
                        f"PITCH: [{pitch_bar}] {frame['pitch']:+06.2f}° | "
                        f"ROLL: [{roll_bar}] {frame['roll']:+06.2f}° | "
                        f"RX: {frame_count} | ERR: {crc_err_count}"
                    )
                    sys.stdout.flush()
                else:
                    crc_err_count += 1
                    rx_buf.pop(0)

            time.sleep(0.005)

    except KeyboardInterrupt:
        print(f"\n\n[INFO] Monitor stopped. Total frames: {frame_count}, CRC errors: {crc_err_count}")
    finally:
        ser.close()

def main():
    parser = argparse.ArgumentParser(description="Live MPU6050 output monitor")
    parser.add_argument("--port", type=str, default="", help="Serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--sim", action="store_true", help="Run in simulation mode without hardware")
    args = parser.parse_args()

    if args.sim:
        run_simulation()
        return

    port = args.port or auto_detect_serial_port()
    if not port:
        print("[WARN] No active serial port (/dev/ttyUSB* or /dev/ttyACM*) detected.")
        print("[INFO] You can run with '--sim' to test the monitor right now:")
        print("       python3 tests/live_mpu_test.py --sim")
        print("\nOnce hardware is plugged in, run without flags:")
        print("       python3 tests/live_mpu_test.py")
        sys.exit(0)

    run_live(port, args.baud)

if __name__ == "__main__":
    main()
