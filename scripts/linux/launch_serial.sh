#!/usr/bin/env bash
# scripts/linux/launch_serial.sh - Linux Serial Terminal Launcher (PuTTY & Hercules Equivalents)
# Target: STM32F103 USART1 @ 115200 8-N-1
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# 1. Probe for serial ports
shopt -s nullglob
PORTS=(/dev/ttyUSB* /dev/ttyACM*)
shopt -u nullglob
if [ ${#PORTS[@]} -eq 0 ]; then
    echo -e "\033[1;31m[ERROR] No USB-UART serial ports found in /dev/ttyUSB* or /dev/ttyACM*.\033[0m" >&2
    echo "Check physical USB connection to CH340 / CP2102 / FTDI / ST-Link adapter." >&2
    exit 1
fi

if [ ${#PORTS[@]} -eq 1 ]; then
    PORT="${PORTS[0]}"
    echo -e "\033[1;32m[INFO] Auto-selected port: ${PORT}\033[0m"
else
    echo "Available Serial Ports:"
    for i in "${!PORTS[@]}"; do
        echo "  [$((i+1))] ${PORTS[$i]}"
    done
    read -rp "Select port number [1-${#PORTS[@]}]: " CHOICE
    if [[ ! "$CHOICE" =~ ^[1-9][0-9]{0,5}$ ]] || ((CHOICE > ${#PORTS[@]})); then
        echo "[ERROR] Invalid port selection." >&2
        exit 2
    fi
    PORT="${PORTS[$((CHOICE-1))]}"
fi

# 2. Check permission for dialout group
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo -e "\033[1;31m[ERROR] Current user (${USER:-<your-user>}) lacks read/write permission on $PORT.\033[0m" >&2
    echo "Add your user to the 'dialout' group and re-login:" >&2
    echo "  sudo usermod -aG dialout \"${USER:-<your-user>}\"" >&2
    exit 1
fi

echo -e "\n\033[1;36m=== Linux Serial Terminal Chooser (Target: $PORT @ 115200 8-N-1) ===\033[0m"
echo "  [1] Python Interactive Shell (Hercules-style [1]-[5] macros, auto-CRLF, colored RX)"
echo "  [2] Picocom CLI (zero-install, lightweight terminal, exit: Ctrl+A Ctrl+X)"
echo "  [3] CuteCom GUI (Exact 1:1 Linux clone of HW-Group Hercules)"
echo "  [4] PuTTY GUI (Native Linux PuTTY)"
echo "  [q] Quit"
read -rp "Choose terminal [1-4/q]: " TOOL_CHOICE

case "$TOOL_CHOICE" in
    1)
        python3 -B "$ROOT_DIR/scripts/windows/monitor_uart.py" -p "$PORT" -b 115200
        ;;
    2)
        if ! command -v picocom >/dev/null 2>&1; then
            echo "[ERROR] picocom is not installed. Install the 'picocom' package and retry." >&2
            exit 1
        fi
        echo "Starting picocom (exit with Ctrl+A then Ctrl+X)..."
        picocom -b 115200 "$PORT" --echo --imap lfcrlf
        ;;
    3)
        if ! command -v cutecom >/dev/null 2>&1; then
            echo "[ERROR] cutecom is not installed. Install the 'cutecom' package and retry." >&2
            exit 1
        fi
        cutecom -s "$PORT" &
        ;;
    4)
        if ! command -v putty >/dev/null 2>&1; then
            echo "[ERROR] putty is not installed. Install the 'putty' package and retry." >&2
            exit 1
        fi
        putty -serial "$PORT" -sercfg 115200,8,n,1,N &
        ;;
    q|Q)
        echo "Exiting."
        exit 0
        ;;
    *)
        echo "[ERROR] Invalid selection." >&2
        exit 2
        ;;
esac
