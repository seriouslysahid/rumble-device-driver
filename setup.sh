#!/bin/bash
# setup.sh — Complete setup for rumble driver project
#
# Usage: sudo ./setup.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRIVER_DIR="$SCRIPT_DIR/driver"
TOOLS_DIR="$SCRIPT_DIR/tools"
SCRIPTS_DIR="$SCRIPT_DIR/scripts"
UDEV_RULE="99-rumble.rules"
UDEV_DEST="/etc/udev/rules.d/$UDEV_RULE"

echo "=== Rumble Driver Setup ==="
echo ""

# Check root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Usage: sudo ./setup.sh"
    exit 1
fi

# Check dependencies
echo "[1/6] Checking dependencies..."
MISSING_DEPS=""

if ! command -v gcc &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS gcc"
fi

if ! command -v make &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS make"
fi

if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    MISSING_DEPS="$MISSING_DEPS linux-headers-$(uname -r)"
fi

if ! dpkg -l | grep -q libncurses-dev; then
    MISSING_DEPS="$MISSING_DEPS libncurses-dev"
fi

if [ -n "$MISSING_DEPS" ]; then
    echo "Missing dependencies:$MISSING_DEPS"
    echo ""
    echo "Install with:"
    echo "  sudo apt update"
    echo "  sudo apt install build-essential linux-headers-\$(uname -r) libncurses-dev"
    exit 1
fi
echo "  ✓ All dependencies present"

# Build driver
echo ""
echo "[2/6] Building kernel module..."
cd "$DRIVER_DIR"
make clean > /dev/null 2>&1 || true
if ! make; then
    echo "Error: Driver build failed"
    exit 1
fi
echo "  ✓ rumble.ko built successfully"

# Build tools
echo ""
echo "[3/6] Building userspace tools..."
cd "$TOOLS_DIR"
make clean > /dev/null 2>&1 || true
if ! make; then
    echo "Error: Tools build failed"
    exit 1
fi
echo "  ✓ rumble_read built"
echo "  ✓ rumble_monitor built"
echo "  ✓ rumble_mouse built"

# Unload xpad if loaded (to avoid conflicts)
echo ""
echo "[4/6] Checking for xpad driver..."
if lsmod | grep -q "^xpad "; then
    echo "  xpad driver loaded (will coexist via driver_override)"
else
    echo "  xpad driver not loaded"
fi

# Load rumble module
echo ""
echo "[5/6] Loading rumble kernel module..."
if lsmod | grep -q "^rumble "; then
    echo "  rumble already loaded, reloading..."
    rmmod rumble || true
fi

if ! insmod "$DRIVER_DIR/rumble.ko"; then
    echo "Error: Failed to load rumble.ko"
    echo "Check dmesg for details: dmesg | grep rumble"
    exit 1
fi
echo "  ✓ rumble.ko loaded"

# Install udev rule
echo ""
echo "[6/6] Installing udev rule..."
cp "$SCRIPTS_DIR/$UDEV_RULE" "$UDEV_DEST"
chmod 644 "$UDEV_DEST"
echo "  ✓ Udev rule installed to $UDEV_DEST"

# Reload udev
udevadm control --reload-rules
udevadm trigger
echo "  ✓ Udev rules reloaded"

# Check if controller is already connected
echo ""
echo "=== Setup Complete ==="
echo ""

if lsusb | grep -q "045e:02dd"; then
    echo "✓ Xbox 1708 controller detected!"
    echo ""
    
    # Try to bind it
    if [ -e "/dev/rumble0" ]; then
        echo "✓ /dev/rumble0 exists and ready"
    else
        echo "Attempting to bind controller..."
        cd "$SCRIPTS_DIR"
        if ./bind.sh; then
            echo "✓ Controller bound successfully"
        else
            echo "⚠ Auto-bind failed, try unplugging and replugging controller"
        fi
    fi
else
    echo "No Xbox 1708 controller detected yet."
    echo "Plug in your controller and it will auto-bind to rumble driver."
fi

echo ""
echo "Next steps:"
echo ""
echo "1. Add your user to input group (for /dev/rumble0 access):"
echo "     sudo usermod -aG input \$USER"
echo "     (then log out and back in)"
echo ""
echo "2. Test the driver:"
echo "     cd tools"
echo "     sudo ./rumble_monitor    # Interactive TUI"
echo "     sudo ./rumble_read       # Simple reader"
echo "     sudo ./rumble_mouse      # Controller-to-mouse mapper"
echo ""
echo "3. Check status:"
echo "     lsmod | grep rumble      # Module loaded?"
echo "     ls -l /dev/rumble0       # Device node exists?"
echo "     dmesg | grep rumble      # Kernel messages"
echo ""
