#!/usr/bin/env bash
# scripts/linux/flash_and_monitor.sh - Compile, Flash & Open Terminal for BTL_KTVXL
# Reuses the terminal spawning and monitor selection from flash_menu.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BAUD="115200"

echo -e "\033[1;36m============================================================\033[0m"
echo -e "\033[1;36m  BTL_KTVXL Inclinometer: Build, Flash & Serial Console      \033[0m"
echo -e "\033[1;36m============================================================\033[0m"
echo "Target: STM32F103 USART1 @ ${BAUD} Baud (8-N-1)"
echo ""

# 1. Compile & flash firmware
echo "--> Compiling and flashing firmware..."
make flash

echo -e "\n\033[1;32m[FLASH SUCCESSFUL]\033[0m"

# 3. Detect active serial port
PORT=""
shopt -s nullglob
PORTS=(/dev/ttyUSB* /dev/ttyACM*)
shopt -u nullglob
if [ -e "/dev/ttyUSB0" ]; then
    PORT="/dev/ttyUSB0"
elif [ ${#PORTS[@]} -gt 0 ]; then
    PORT="${PORTS[0]}"
fi

if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
    echo -e "\033[1;33m[WARN] No USB-UART serial port detected (/dev/ttyUSB* or /dev/ttyACM*).\033[0m" >&2
    exit 1
fi

# Verify port read/write permissions without mutating device permissions
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo -e "\033[1;31m[ERROR] Missing read/write permission on $PORT.\033[0m" >&2
    echo "Add your user to the 'dialout' group and re-login:" >&2
    echo "  sudo usermod -aG dialout \"${USER:-<your-user>}\"" >&2
    exit 1
fi

# 4. Prompt to open terminal (reused from flash_menu.sh)
echo ""
echo "Do you want to open a serial monitor now?"
echo "  [1] Launch PuTTY GUI in background"
echo "  [2] Spawn monitor in a NEW separate terminal window (gnome-terminal/xterm)"
echo "  [3] Open monitor HERE (in this terminal window via picocom)"
echo "  [4] Launch CuteCom GUI in background"
echo "  [n] No, return to shell"
read -rp "Select option [1-4/n]: " MON_OPT

case "$MON_OPT" in
    1)
        if ! command -v putty >/dev/null 2>&1; then
            echo -e "\033[1;33m[ERROR] PuTTY is not installed. Install the 'putty' package and retry.\033[0m" >&2
            exit 1
        fi
        putty -serial "$PORT" -sercfg "${BAUD},8,n,1,N" >/dev/null 2>&1 &
        echo "Launched PuTTY GUI on $PORT @ $BAUD in background (PID: $!)."
        ;;
    2)
        if ! command -v picocom >/dev/null 2>&1; then
            echo -e "\033[1;31m[ERROR] picocom is not installed. Install the 'picocom' package and retry.\033[0m" >&2
            exit 1
        fi
        if command -v gnome-terminal >/dev/null 2>&1; then
            gnome-terminal --title="BTL_KTVXL Monitor ($BAUD)" -- picocom -b "$BAUD" "$PORT" --echo --imap lfcrlf &
            echo "Spawned new terminal window on $PORT @ $BAUD baud."
        elif command -v xterm >/dev/null 2>&1; then
            xterm -T "BTL_KTVXL Monitor" -e picocom -b "$BAUD" "$PORT" --echo --imap lfcrlf &
            echo "Spawned xterm window."
        else
            echo "Neither gnome-terminal nor xterm found. Running in current terminal instead:"
            picocom -b "$BAUD" "$PORT" --echo --imap lfcrlf
        fi
        ;;
    3)
        if ! command -v picocom >/dev/null 2>&1; then
            echo -e "\033[1;31m[ERROR] picocom is not installed. Install the 'picocom' package and retry.\033[0m" >&2
            exit 1
        fi
        picocom -b "$BAUD" "$PORT" --echo --imap lfcrlf
        ;;
    4)
        if ! command -v cutecom >/dev/null 2>&1; then
            echo -e "\033[1;33m[ERROR] CuteCom is not installed. Install the 'cutecom' package and retry.\033[0m" >&2
            exit 1
        fi
        cutecom -s "$PORT" >/dev/null 2>&1 &
        echo "Launched CuteCom GUI in background."
        ;;
    *)
        echo "Done."
        ;;
esac
