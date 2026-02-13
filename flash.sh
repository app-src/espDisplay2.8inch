#!/bin/bash

# ==============================================================================
# Script to Compile and Flash ESP32 Firmware using Arduino CLI
# ==============================================================================

# ---------------- Configuration ----------------
# FQBN (Fully Qualified Board Name)
# Based on your pinout (Pins 40+), you are likely using an ESP32-S3.
# Common FQBNs:
# - Generic ESP32-S3:      esp32:esp32:esp32s3
# - Generic ESP32:         esp32:esp32:esp32
# - ESP32-C3:              esp32:esp32:esp32c3
BOARD_FQBN="esp32:esp32:esp32s3"

# Start
set -e # Exit immediately if a command exits with a non-zero status.

echo "=========================================="
echo "    ESP32 Build & Flash Script"
echo "=========================================="

# 1. Check for arduino-cli
# Add local user PATH for current session
export PATH=${PWD}/bin:$PATH

if ! command -v arduino-cli &> /dev/null; then
    echo "❌ Error: arduino-cli command not found."
    echo "Please install it: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

# 2. Check/Install ESP32 Core
echo "Checking ESP32 core..."
STATUS=$(arduino-cli core list | grep "esp32:esp32" || true)
if [ -z "$STATUS" ]; then
    echo "⚠️ ESP32 core not found. Installing..."
    arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
else
    echo "✅ ESP32 core is installed."
fi

# 3. Detect Port
echo "Searching for connected board..."
arduino-cli board list
# Try to auto-detect the first USB serial device (Linux specific)
PORT=$(arduino-cli board list | grep "/dev/tty" | awk '{print $1}' | head -n 1)

if [ -z "$PORT" ]; then
    echo "⚠️ No board auto-detected."
    read -p "Please enter your serial port (e.g., /dev/ttyACM0): " PORT
else
    echo "✅ Auto-detected port: $PORT"
    read -p "Press ENTER to use $PORT or type a new port: " USER_PORT
    if [ ! -z "$USER_PORT" ]; then
        PORT="$USER_PORT"
    fi
fi

# 4. Compile
echo "Compiling sketch for $BOARD_FQBN..."
arduino-cli compile --fqbn $BOARD_FQBN .

# 5. Upload
echo "Uploading to $PORT..."
arduino-cli upload -p "$PORT" --fqbn $BOARD_FQBN .

echo "=========================================="
echo "🎉 Done! Opening Serial Monitor..."
echo "Press Ctrl+C to exit monitor."
echo "=========================================="

# 6. Monitor
arduino-cli monitor -p "$PORT" --config baudrate=115200
