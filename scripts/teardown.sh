#!/bin/bash
# teardown.sh — Unload rumble driver and remove udev rules
#
# Usage: sudo ./teardown.sh

set -e

UDEV_RULE="/etc/udev/rules.d/99-rumble.rules"

echo "=== Rumble Driver Teardown ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Usage: sudo ./teardown.sh"
    exit 1
fi

# Unload module
if lsmod | grep -q "^rumble "; then
    echo "Unloading rumble kernel module..."
    rmmod rumble
    echo "Module unloaded"
else
    echo "rumble module not loaded"
fi

# Remove udev rule
if [ -f "$UDEV_RULE" ]; then
    echo "Removing udev rule..."
    rm -f "$UDEV_RULE"
    echo "Udev rule removed"
    
    echo "Reloading udev rules..."
    udevadm control --reload-rules
    udevadm trigger
else
    echo "Udev rule not installed"
fi

echo ""
echo "=== Teardown Complete ==="
echo ""
echo "The rumble driver has been unloaded."
echo "Xbox controllers will now use the default xpad driver."
echo ""
