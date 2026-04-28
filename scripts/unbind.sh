#!/bin/bash
# unbind.sh — Unbind controller from rumble and restore xpad
#
# Usage: sudo ./unbind.sh

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
    echo "Error: Xbox 1708 controller not found"
    exit 1
fi

DEVNAME=$(basename "$DEVPATH")
echo "Found controller: $DEVNAME"

# Unbind from rumble if bound
if [ -e "$DEVPATH/driver" ]; then
    CURRENT_DRIVER=$(basename "$(readlink "$DEVPATH/driver")")
    if [ "$CURRENT_DRIVER" = "rumble" ]; then
        echo "Unbinding from rumble driver..."
        echo "$DEVNAME" > "$DEVPATH/driver/unbind"
    fi
fi

# Clear driver override
echo "" > "$DEVPATH/driver_override"
echo "Cleared driver_override"

# Rebind to xpad (kernel will auto-bind)
echo "Triggering rebind..."
echo "$DEVNAME" > /sys/bus/usb/drivers_probe

echo "Success! Controller should now use xpad driver."
