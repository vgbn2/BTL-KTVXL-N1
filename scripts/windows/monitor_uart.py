#!/usr/bin/env python3
"""
scripts/windows/monitor_uart.py - Interactive UART Telemetry Monitor & Command Console
Target: STM32F103 USART1 (115200 Baud, 8-N-1, CRLF)
Platform: Windows, Linux, macOS
"""

import sys
import time
import argparse
import threading
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] 'pyserial' package is not installed.")
    print("Run: pip install pyserial")
    print("Or on Windows without Python packages, use scripts/windows/serial_terminal.ps1 instead.")
    sys.exit(1)


def find_com_ports():
    """List available serial COM ports."""
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]


def reader_thread(ser, stop_event, log_file=None):
    """Continuously read lines from UART and print to stdout."""
    log_fp = None
    if log_file:
        try:
            log_fp = open(log_file, "a", encoding="utf-8")
            log_fp.write(f"# Session started at {datetime.now().isoformat()}\n")
            log_fp.flush()
        except Exception as e:
            print(f"[WARN] Failed to open log file {log_file}: {e}")

    try:
        while not stop_event.is_set():
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    print(f"\r\033[32m[{ts} RX] {line}\033[0m\nSTM32-CLI> ", end="", flush=True)
                    if log_fp:
                        log_fp.write(f"{ts},{line}\n")
                        log_fp.flush()
            else:
                time.sleep(0.01)
    except Exception as e:
        if not stop_event.is_set():
            print(f"\n[RX ERROR] {e}")
    finally:
        if log_fp:
            log_fp.close()


def main():
    parser = argparse.ArgumentParser(description="STM32F103 UART Telemetry Monitor & Command Shell")
    parser.add_argument("-p", "--port", type=str, default=None, help="COM port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-l", "--log", type=str, default=None, help="Optional log file path")
    args = parser.parse_args()

    port = args.port
    if not port:
        ports = find_com_ports()
        if not ports:
            print("[ERROR] No serial COM ports detected.")
            print("Ensure your USB-to-UART bridge (CH340, CP2102, FT232, ST-Link) is connected.")
            sys.exit(1)
        if len(ports) == 1:
            port = ports[0]
            print(f"[INFO] Auto-selected COM port: {port}")
        else:
            print("Available COM ports:")
            for idx, p in enumerate(ports, 1):
                print(f"  [{idx}] {p}")
            choice = input(f"Select port [1-{len(ports)}] or type port name: ").strip()
            if choice.isdigit() and 1 <= int(choice) <= len(ports):
                port = ports[int(choice) - 1]
            else:
                port = choice

    print(f"\nConnecting to {port} @ {args.baud} baud (8-N-1)...")
    try:
        ser = serial.Serial(
            port=port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.2,
            write_timeout=1.0
        )
    except Exception as e:
        print(f"[FATAL] Cannot open {port}: {e}")
        print("Check if port is already open in PuTTY, Hercules, or IDE serial monitor.")
        sys.exit(1)

    print("\033[1;36m=====================================================================\033[0m")
    print("\033[1;36m  STM32F103 UART TELEMETRY MONITOR & COMMAND SHELL                  \033[0m")
    print("\033[1;36m  115200 8-N-1 | Framing: CRLF (\\r\\n)                                \033[0m")
    print("\033[1;36m=====================================================================\033[0m")
    print("Quick Command Shortcuts:")
    print("  [1] INFO_REQUEST         - Query group & project metadata")
    print("  [2] GET_ANGLE            - Read instantaneous Roll & Pitch angles")
    print("  [3] CALIBRATE_IMU        - Run static IMU calibration & save to Flash")
    print("  [4] SET_FILTER_ALPHA:0.96 - Set complementary filter alpha to 0.96")
    print("  [5] SET_FILTER_ALPHA:0.98 - Set complementary filter alpha to 0.98")
    print("  [q] Quit session")
    print("  Or type any custom command directly (e.g. 'SET_FILTER_ALPHA:0.95').\n")

    stop_event = threading.Event()
    rx_th = threading.Thread(target=reader_thread, args=(ser, stop_event, args.log), daemon=True)
    rx_th.start()

    shortcuts = {
        "1": "INFO_REQUEST",
        "2": "GET_ANGLE",
        "3": "CALIBRATE_IMU",
        "4": "SET_FILTER_ALPHA:0.96",
        "5": "SET_FILTER_ALPHA:0.98",
    }

    try:
        while True:
            cmd = input("STM32-CLI> ").strip()
            if not cmd:
                continue
            if cmd.lower() in ["q", "quit", "exit"]:
                break

            actual_cmd = shortcuts.get(cmd, cmd)
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"\033[33m[{ts} TX] {actual_cmd}\033[0m")
            # Send with CRLF
            ser.write((actual_cmd + "\r\n").encode("utf-8"))
            ser.flush()
            time.sleep(0.1)
    except (KeyboardInterrupt, EOFError):
        print("\n[INFO] Interrupt received, exiting...")
    finally:
        stop_event.set()
        rx_th.join(timeout=0.5)
        ser.close()
        print(f"[INFO] Port {port} closed cleanly.")


if __name__ == "__main__":
    main()
