#!/bin/bash
# bind.sh — Bind Xbox 1708 controller to rumble driver
#
# Usage: sudo ./bind.sh

set -e

VENDOR="045e"
PRODUCT="02dd"

# Find the USB device
DEVPATH=$(find /sys/bus/usb/devices -name "idVendor" -exec grep -l "$VENDOR" {} \; | \
          xargs -I{} dirname {} | \
          while read dev; do
              if grep -q "$PRODUCT" "$dev/idProduct" 2>/dev/null; then
                  echo "$dev"
                  break
              fi
          done)

if [ -z "$DEVPATH" ]; then
    echo "Error: Xbox 1708 controller not found (VID=$VENDOR PID=$PRODUCT)"
    echo "Hint: Is the controller plugged in?"
    exit 1
fi

DEVNAME=$(basename "$DEVPATH")
echo "Found controller: $DEVNAME"

# Set driver override
echo "rumble" > "$DEVPATH/driver_override"
echo "Set driver_override to 'rumble'"

# Unbind from current driver if bound
if [ -e "$DEVPATH/driver" ]; then
    CURRENT_DRIVER=$(basename "$(readlink "$DEVPATH/driver")")
    echo "Unbinding from $CURRENT_DRIVER..."
    echo "$DEVNAME" > "$DEVPATH/driver/unbind" 2>/dev/null || true
fi

# Bind to rumble driver
if [ -e "/sys/bus/usb/drivers/rumble/bind" ]; then
    echo "Binding to rumble driver..."
    echo "$DEVNAME" > /sys/bus/usb/drivers/rumble/bind
    echo "Success! Controller bound to rumble driver."
    echo "Device node: /dev/rumble0"
else
    echo "Error: rumble driver not loaded"
    echo "Hint: sudo insmod ../driver/rumble.ko"
    exit 1
fi
