# Rumble Driver Scripts

Automation scripts for loading the rumble driver and managing controller binding.

## Quick Start

```bash
# Build the driver first
cd ../driver && make && cd ../scripts

# Install driver + udev rules
sudo ./setup.sh

# Plug in Xbox 1708 controller (auto-binds to rumble)

# Test
cd ../tools && sudo ./rumble_monitor
```

## Scripts

### `setup.sh`
Installs the rumble driver and udev rules for automatic binding.

**What it does:**
- Loads `rumble.ko` kernel module
- Installs udev rule to `/etc/udev/rules.d/99-rumble.rules`
- Reloads udev rules
- Configures auto-binding for Xbox 1708 controllers

**Usage:**
```bash
sudo ./setup.sh
```

### `teardown.sh`
Removes the rumble driver and udev rules.

**What it does:**
- Unloads `rumble.ko` kernel module
- Removes udev rule
- Reloads udev rules
- Controllers revert to xpad driver

**Usage:**
```bash
sudo ./teardown.sh
```

### `bind.sh`
Manually binds an Xbox 1708 controller to the rumble driver.

**Usage:**
```bash
sudo ./bind.sh
```

**When to use:**
- Testing without installing udev rules
- Temporarily switching from xpad to rumble
- Demonstrating sysfs driver binding

### `unbind.sh`
Manually unbinds controller from rumble and restores xpad.

**Usage:**
```bash
sudo ./unbind.sh
```

## How It Works

### Driver Binding Mechanism

The scripts use the Linux `driver_override` mechanism:

1. **driver_override**: Write "rumble" to `/sys/bus/usb/devices/<dev>/driver_override`
2. **Unbind**: Detach from current driver (xpad)
3. **Bind**: Attach to rumble driver

This is the modern, clean approach (kernel 4.0+) and doesn't require blacklisting xpad globally.

### Udev Rule

The udev rule (`99-rumble.rules`) automatically:
- Sets `driver_override="rumble"` when Xbox 1708 is plugged in
- Triggers binding to rumble driver
- Sets `/dev/rumble0` permissions to `0660` with group `input`

### Permissions

To use `/dev/rumble0` without sudo:
```bash
sudo usermod -aG input $USER
# Log out and back in
```

## Troubleshooting

### Controller not binding
```bash
# Check if rumble module is loaded
lsmod | grep rumble

# Check if controller is detected
lsusb | grep 045e

# Check driver binding
ls -l /sys/bus/usb/drivers/rumble/
```

### /dev/rumble0 not appearing
```bash
# Check dmesg for errors
dmesg | grep rumble

# Verify module loaded successfully
lsmod | grep rumble

# Check device permissions
ls -l /dev/rumble0
```

### Reverting to xpad
```bash
# Option 1: Use unbind script
sudo ./unbind.sh

# Option 2: Remove udev rule and replug
sudo ./teardown.sh
# Unplug and replug controller
```

## Technical Details

### USB Device IDs
- Vendor ID: `045e` (Microsoft)
- Product ID: `02dd` (Xbox Wireless Controller Model 1708)

### Sysfs Paths
- Driver override: `/sys/bus/usb/devices/<dev>/driver_override`
- Bind: `/sys/bus/usb/drivers/rumble/bind`
- Unbind: `/sys/bus/usb/devices/<dev>/driver/unbind`

### Character Device
- Device node: `/dev/rumble0`
- Major: dynamically allocated
- Minor: 0
- Class: `rumble`
