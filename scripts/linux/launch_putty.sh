#!/usr/bin/env bash
# scripts/linux/launch_putty.sh - Direct PuTTY Launcher for BTL_KTVXL Telemetry
# Target: STM32F103 USART1 @ 115200 Baud, 8-N-1
set -euo pipefail

BAUD=115200
USE_CLI=0
PORT=""

usage() {
    echo "Usage: $0 [--cli|-c] [--port /dev/ttyUSB0 | /dev/ttyUSB0]"
    echo "PuTTY (or plink with --cli): 115200 baud, 8N1, no flow control."
    echo "Without a port, discover USB/ACM ports and prompt if multiple exist."
}

while (($#)); do
    case "$1" in
        --cli|-c) USE_CLI=1; shift ;;
        -h|--help) usage; exit 0 ;;
        --port)
            if (($# < 2)) || [[ -z "$2" || "$2" == -* ]]; then
                echo "ERROR: --port requires a device path." >&2
                exit 2
            fi
            PORT="$2"; shift 2 ;;
        /dev/*) PORT="$1"; shift ;;
        *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

# 1. Check tools
if [ $USE_CLI -eq 1 ]; then
    if ! command -v plink >/dev/null 2>&1; then
        echo "ERROR: plink (PuTTY CLI) is not available in PATH." >&2
        exit 1
    fi
else
    if ! command -v putty >/dev/null 2>&1; then
        echo "ERROR: putty is not available in PATH." >&2
        exit 1
    fi
fi

# 2. Probe for active serial ports if not explicitly specified
if [ -z "$PORT" ]; then
    shopt -s nullglob
    PORTS=(/dev/ttyUSB* /dev/ttyACM*)
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo -e "\033[1;31m[ERROR] No serial ports found in /dev/ttyUSB* or /dev/ttyACM*.\033[0m"
        echo "Check USB-UART adapter connection to PC."
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
        read -rp "Select port [1-${#PORTS[@]}]: " CHOICE
        if [[ ! "$CHOICE" =~ ^[1-9][0-9]{0,5}$ ]] || ((CHOICE > ${#PORTS[@]})); then
            echo "ERROR: invalid port selection." >&2
            exit 2
        fi
        PORT="${PORTS[$((CHOICE-1))]}"
    fi
fi

# 3. Ensure read/write access
if [ ! -c "$PORT" ]; then
    echo "ERROR: serial device does not exist or is not a character device: $PORT" >&2
    exit 1
fi
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo "ERROR: read/write access required for $PORT; ask the device administrator for access." >&2
    exit 1
fi

# 4. Launch either PuTTY GUI or PuTTY CLI (plink)
if [ $USE_CLI -eq 1 ]; then
    echo -e "\033[1;32m--> Launching PuTTY CLI (plink) on ${PORT} @ ${BAUD} baud (8-N-1)... [Ctrl+C to exit]\033[0m"
    exec plink -serial "$PORT" -sercfg "${BAUD},8,n,1,N"
else
    echo -e "\033[1;32m--> Launching PuTTY GUI on ${PORT} @ ${BAUD} baud (8-N-1)...\033[0m"
    exec putty -serial "$PORT" -sercfg "${BAUD},8,n,1,N"
fi
