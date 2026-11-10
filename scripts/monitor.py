#!/usr/bin/env python3
"""
scripts/monitor.py - Live Telemetry Monitor for BTL-KTVXL Inclinometer.
Streams pitch, roll, and accelerometer readings in real time.
Supports both SWD (ST-Link V2) and UART (/dev/ttyUSB*).
"""

import os
import sys
import time
import socket
import struct
import glob
import subprocess

def monitor_uart(port, baud=115200):
    import serial
    print(f"\033[1;36m[MONITOR] Listening on UART serial port {port} @ {baud} baud...\033[0m")
    with serial.Serial(port, baud, timeout=1) as ser:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"\033[32m{line}\033[0m")

def monitor_swd():
    print("\033[1;36m============================================================\033[0m")
    print("\033[1;36m    STM32 MPU6050 LIVE TELEMETRY MONITOR (ST-LINK SWD)     \033[0m")
    print("\033[1;36m============================================================\033[0m")
    print("Connecting to STM32 over ST-Link SWD...")

    openocd_proc = subprocess.Popen([
        "openocd", "-f", "interface/stlink.cfg", "-f", "target/stm32f1x.cfg",
        "-c", "tcl_port 6666", "-c", "gdb_port 0", "-c", "telnet_port 0", "-c", "init"
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    time.sleep(0.4)
    s = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(("127.0.0.1", 6666))

        print("\033[1;32m[CONNECTED] Streaming live sensor data (Press Ctrl+C to stop):\033[0m\n")

        last_count = -1
        while True:
            s.sendall(b"mdw 0x20000000 6\x1a")
            resp = b""
            while b"\x1a" not in resp:
                chunk = s.recv(512)
                if not chunk:
                    break
                resp += chunk

            text = resp.decode(errors="ignore").strip()
            lines = text.split("\n")
            if lines and lines[0].startswith("0x20000000:"):
                parts = lines[0].split()
                if len(parts) >= 7:
                    words = [int(w, 16) for w in parts[1:7]]
                    buf = struct.pack("<6I", *words)
                    (p_x100, r_x100, ax, ay, az,
                     temp, gx, gy, gz, is_real, count) = struct.unpack("<hhhhhhHHhHI", buf[:24])

                    if count != last_count:
                        last_count = count
                        pitch = p_x100 / 100.0
                        roll = r_x100 / 100.0
                        tag = "\033[1;32m[HARDWARE REAL]\033[0m" if is_real else "\033[1;33m[SIMULATED]\033[0m"
                        sys.stdout.write(
                            f"\r\033[K#{count:05d} | Pitch: \033[1;33m{pitch:+06.2f}°\033[0m | "
                            f"Roll: \033[1;36m{roll:+06.2f}°\033[0m | "
                            f"Accel: (X={ax:+6d}, Y={ay:+6d}, Z={az:+6d}) | {tag}"
                        )
                        sys.stdout.flush()

            time.sleep(0.08)

    except KeyboardInterrupt:
        print("\n\n\033[1;33m[STOPPED] Monitoring finished.\033[0m")
    finally:
        if s:
            try:
                s.sendall(b"shutdown\x1a")
                s.close()
            except Exception:
                pass
        openocd_proc.terminate()
        try:
            openocd_proc.wait(timeout=1.0)
        except Exception:
            openocd_proc.kill()

def main():
    ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    if ports:
        try:
            monitor_uart(ports[0])
            return
        except Exception as e:
            print(f"[WARN] Could not open UART port: {e}, falling back to SWD...")

    monitor_swd()

if __name__ == "__main__":
    main()
