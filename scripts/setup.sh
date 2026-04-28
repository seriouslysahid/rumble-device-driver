#!/bin/bash
# setup.sh — Install rumble driver and udev rules
#
# Usage: sudo ./setup.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRIVER_DIR="$SCRIPT_DIR/../driver"
UDEV_RULE="99-rumble.rules"
UDEV_DEST="/etc/udev/rules.d/$UDEV_RULE"

echo "=== Rumble Driver Setup ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Usage: sudo ./setup.sh"
    exit 1
fi

# Check if driver module exists
if [ ! -f "$DRIVER_DIR/rumble.ko" ]; then
    echo "Error: rumble.ko not found"
    echo "Hint: cd ../driver && make"
    exit 1
fi

# Load the module
echo "Loading rumble kernel module..."
insmod "$DRIVER_DIR/rumble.ko"
echo "Module loaded successfully"

# Install udev rule
echo "Installing udev rule..."
cp "$SCRIPT_DIR/$UDEV_RULE" "$UDEV_DEST"
chmod 644 "$UDEV_DEST"
echo "Udev rule installed to $UDEV_DEST"

# Reload udev rules
echo "Reloading udev rules..."
udevadm control --reload-rules
udevadm trigger

echo ""
echo "=== Setup Complete ==="
echo ""
echo "The rumble driver is now loaded and will auto-bind to Xbox 1708 controllers."
echo ""
echo "To add your user to the input group (for /dev/rumble0 access):"
echo "  sudo usermod -aG input \$USER"
echo "  (then log out and back in)"
echo ""
echo "To test:"
echo "  1. Plug in Xbox 1708 controller"
echo "  2. Check: ls -l /dev/rumble0"
echo "  3. Run: cd ../tools && sudo ./rumble_monitor"
echo ""
