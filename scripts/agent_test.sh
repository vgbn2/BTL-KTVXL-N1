#!/usr/bin/env bash
# Offline host verification; firmware compilation is explicitly opt-in.
set -euo pipefail

usage() {
    echo "Usage: $0 [--firmware] [--build-dir PATH]"
    echo "Runs native CMake/CTest, src flash/UART boundary checks, and agent_uart self-test."
    echo "--firmware also compiles/links ARM firmware; never flashes hardware."
    echo "Artifacts: PATH (relative to caller), or a fresh /tmp/opencode/btl-agent.* directory."
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR=""
FIRMWARE=0
while (($#)); do
    case "$1" in
        --firmware) FIRMWARE=1; shift ;;
        --build-dir)
            if (($# < 2)) || [[ -z "$2" || "$2" == --* ]]; then
                echo "ERROR: --build-dir requires a path." >&2
                exit 2
            fi
            BUILD_DIR="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

STAGE="setup"
trap 'status=$?; if ((status)); then echo "SUMMARY: FAIL at $STAGE (exit $status); artifacts: ${BUILD_DIR:-not created}" >&2; fi' EXIT
for tool in cmake ctest python3 cc; do
    command -v "$tool" >/dev/null || { echo "ERROR: missing executable: $tool" >&2; exit 1; }
done
if ((FIRMWARE)); then
    command -v make >/dev/null || { echo "ERROR: missing executable: make" >&2; exit 1; }
fi
if [[ -z "$BUILD_DIR" ]]; then
    mkdir -p /tmp/opencode
    BUILD_DIR="$(mktemp -d /tmp/opencode/btl-agent.XXXXXX)"
fi
mkdir -p -- "$BUILD_DIR"
BUILD_DIR="$(cd -- "$BUILD_DIR" && pwd)"
mkdir -p "$BUILD_DIR/tmp"
export TMPDIR="$BUILD_DIR/tmp" PYTHONDONTWRITEBYTECODE=1

STAGE="host configure"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR/host" -DCMAKE_BUILD_TYPE=Debug
STAGE="host build"
cmake --build "$BUILD_DIR/host" --parallel 2
STAGE="host CTest"
(cd "$BUILD_DIR/host" && ctest --output-on-failure --no-tests=error --parallel 1)
STAGE="src flash/UART boundaries"
python3 -B "$ROOT_DIR/tests/repro/check_flash_uart_boundaries.py" src
STAGE="agent_uart self-test"
python3 -B "$ROOT_DIR/scripts/agent_uart.py" --self-test
if ((FIRMWARE)); then
    STAGE="ARM firmware compile/link"
    make -C "$ROOT_DIR" "BUILD_DIR=$BUILD_DIR" firmware
    echo "SUMMARY: PASS — host CTest, src boundaries, agent_uart self-test, ARM compile/link; artifacts: $BUILD_DIR"
else
    echo "SUMMARY: PASS — host CTest, src boundaries, agent_uart self-test; ARM build not requested; artifacts: $BUILD_DIR"
fi
