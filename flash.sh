#!/bin/bash
# Compile and flash Waveshare ESP32-S3-POE-ETH-8DI-8RO firmware
#
# Modified firmware includes:
#   WS_RS485.cpp — RS485 DI query handler (06 01 00... → DI bitmask response)
#   WS_DIN.h    — Relay_Immediate_Default = 0 (DI→relay auto-mirroring disabled)
#
# Usage:
#   ./flash.sh                        # port auto-detected, device address 1
#   ./flash.sh --port /dev/ttyACM1   # explicit port
#   ./flash.sh --address 2           # flash as device address 2 (for multi-unit bus)

set -e

FIRMWARE_DIR="$(cd "$(dirname "$0")/firmware/MAIN_ALL" && pwd)"
ARDUINO_CLI="$HOME/.local/bin/arduino-cli"
DEVICE_ADDRESS=1
PORT=""

# ── Parse args ────────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)     PORT="$2";           shift 2 ;;
        --address)  DEVICE_ADDRESS="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "================================"
echo " ESP32 Firmware Compile & Flash"
echo "================================"
echo " Device address : $DEVICE_ADDRESS"
echo " Firmware dir   : $FIRMWARE_DIR"
echo

# ── Step 1: arduino-cli ───────────────────────────────────────────────────────
if [ ! -f "$ARDUINO_CLI" ]; then
    echo "Step 1: Installing Arduino CLI..."
    mkdir -p "$HOME/.local/bin"
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$HOME/.local/bin" sh
    echo "✓ Arduino CLI installed"
else
    echo "Step 1: Arduino CLI already present"
fi
export PATH="$HOME/.local/bin:$PATH"
echo

# ── Step 2: ESP32 board support ───────────────────────────────────────────────
ESP32_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
echo "Step 2: Checking ESP32 board support..."
if ! $ARDUINO_CLI core list 2>/dev/null | grep -q "esp32:esp32"; then
    echo "  Updating board index..."
    $ARDUINO_CLI core update-index --additional-urls "$ESP32_URL"
    echo "  Installing ESP32 core (a few minutes)..."
    $ARDUINO_CLI core install esp32:esp32 --additional-urls "$ESP32_URL"
    echo "✓ ESP32 core installed"
else
    echo "✓ ESP32 core already installed"
fi
echo

# ── Step 3: Libraries ─────────────────────────────────────────────────────────
echo "Step 3: Checking required libraries..."
for LIB in "ArduinoJson" "PubSubClient" "NTPClient"; do
    if ! $ARDUINO_CLI lib list 2>/dev/null | grep -q "$LIB"; then
        echo "  Installing $LIB..."
        $ARDUINO_CLI lib install "$LIB"
    fi
    echo "  ✓ $LIB"
done
echo

# ── Step 4: Set device address ────────────────────────────────────────────────
echo "Step 4: Setting DEVICE_ADDRESS to $DEVICE_ADDRESS in WS_RS485.h..."
sed -i "s/^#define DEVICE_ADDRESS.*/#define DEVICE_ADDRESS      0x0${DEVICE_ADDRESS}/" "$FIRMWARE_DIR/WS_RS485.h"
echo "✓ DEVICE_ADDRESS = 0x0${DEVICE_ADDRESS}"
echo

# ── Step 5: Compile ───────────────────────────────────────────────────────────
echo "Step 5: Compiling firmware..."
mkdir -p "$FIRMWARE_DIR/build"
$ARDUINO_CLI compile \
    --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app \
    --additional-urls "$ESP32_URL" \
    --output-dir "$FIRMWARE_DIR/build" \
    "$FIRMWARE_DIR/MAIN_ALL.ino"
echo "✓ Compilation successful"
echo "  Binary: $FIRMWARE_DIR/build/MAIN_ALL.ino.merged.bin"
echo

# ── Step 6: Detect port ───────────────────────────────────────────────────────
if [ -z "$PORT" ]; then
    echo "Step 6: Auto-detecting ESP32 USB port..."
    for candidate in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/ttyUSB1; do
        if [ -e "$candidate" ]; then
            PORT="$candidate"
            echo "  Found: $PORT"
            break
        fi
    done
    if [ -z "$PORT" ]; then
        echo "ERROR: No USB serial port found. Connect the ESP32 USB cable and retry."
        echo "       Or specify the port manually: ./flash.sh --port /dev/ttyACM0"
        exit 1
    fi
else
    echo "Step 6: Using port $PORT"
    if [ ! -e "$PORT" ]; then
        echo "ERROR: Port $PORT not found."
        exit 1
    fi
fi
echo

# ── Step 7: Flash ─────────────────────────────────────────────────────────────
echo "Step 7: Ready to flash device $DEVICE_ADDRESS via $PORT"
echo "        Make sure the ESP32 USB cable is connected."
echo
read -p "Press Enter to flash (Ctrl+C to cancel)..."

esptool \
    --chip esp32s3 \
    --port "$PORT" \
    --baud 460800 \
    --no-stub \
    --before no-reset \
    --after hard-reset \
    write-flash \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size 16MB \
    0x0 "$FIRMWARE_DIR/build/MAIN_ALL.ino.merged.bin"

echo
echo "================================"
echo "  Flash complete! Device $DEVICE_ADDRESS"
echo "================================"
echo
echo "Next steps:"
echo "  Disconnect the USB cable from the ESP32."
echo "  Connect the RS485 dongle and test:"
echo "    python3 esp32_control.py status"
echo "    python3 esp32_control.py poll"
echo
