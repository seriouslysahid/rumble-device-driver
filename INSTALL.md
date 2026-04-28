# Installation Guide

Complete installation and setup instructions for the rumble driver.

---

## Prerequisites

### Hardware
- Xbox Wireless Controller Model 1708 (wired USB)
- USB cable
- Linux machine (Ubuntu 22.04+ recommended)

### Software
- Linux kernel 6.4+ with headers
- GCC compiler
- Make
- ncurses development headers

---

## Quick Install

```bash
sudo ./setup.sh
```

This single command will:
1. Check all dependencies
2. Build kernel module
3. Build userspace tools
4. Load the driver
5. Install udev rules
6. Configure auto-binding

---

## Manual Installation

If you prefer step-by-step installation:

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) libncurses-dev
```

**Fedora:**
```bash
sudo dnf install gcc make kernel-devel ncurses-devel
```

**Arch:**
```bash
sudo pacman -S base-devel linux-headers ncurses
```

### 2. Build

```bash
# Build kernel module
cd driver
make

# Build userspace tools
cd ../tools
make
```

### 3. Load Driver

```bash
cd ../driver
sudo insmod rumble.ko
```

Verify:
```bash
lsmod | grep rumble
dmesg | grep rumble
```

### 4. Install Udev Rules

```bash
cd ../scripts
sudo cp 99-rumble.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 5. Bind Controller

**Option A: Automatic (udev)**
- Unplug and replug controller
- Should auto-bind to rumble driver

**Option B: Manual**
```bash
cd scripts
sudo ./bind.sh
```

### 6. Set Permissions

```bash
sudo usermod -aG input $USER
```

**Important:** Log out and back in for group changes to take effect.

---

## Verification

### Check Module
```bash
lsmod | grep rumble
```

Expected output:
```
rumble                 16384  0
```

### Check Device Node
```bash
ls -l /dev/rumble0
```

Expected output:
```
crw-rw---- 1 root input 243, 0 Jan 1 12:00 /dev/rumble0
```

### Check Controller
```bash
lsusb | grep 045e
```

Expected output:
```
Bus 001 Device 003: ID 045e:02dd Microsoft Corp. Xbox One Controller
```

### Check Driver Binding
```bash
ls -l /sys/bus/usb/drivers/rumble/
```

Should show a symlink to your controller device.

### Test Tools
```bash
cd tools

# Simple reader
sudo ./rumble_read

# Interactive monitor
sudo ./rumble_monitor

# Mouse mapper
sudo ./rumble_mouse
```

---

## Troubleshooting

### "No such device"

**Cause:** Module not loaded or controller not connected

**Fix:**
```bash
# Check module
lsmod | grep rumble

# Load if missing
cd driver && sudo insmod rumble.ko

# Check controller
lsusb | grep 045e
```

### "Permission denied"

**Cause:** User not in input group

**Fix:**
```bash
sudo usermod -aG input $USER
# Log out and back in
```

Or use sudo:
```bash
sudo ./rumble_read
```

### "Device or resource busy"

**Cause:** Controller bound to xpad driver

**Fix:**
```bash
cd scripts
sudo ./bind.sh
```

### "Cannot allocate memory" (kernel)

**Cause:** Out of kernel memory or DMA buffer allocation failed

**Fix:**
- Check dmesg: `dmesg | grep rumble`
- Free memory or reboot
- Check for memory leaks in other drivers

### Controller not auto-binding

**Cause:** Udev rule not installed or not triggered

**Fix:**
```bash
# Check rule exists
cat /etc/udev/rules.d/99-rumble.rules

# Reload rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Unplug and replug controller
```

### Build fails with "kernel headers not found"

**Cause:** Kernel headers don't match running kernel

**Fix:**
```bash
# Check kernel version
uname -r

# Install matching headers
sudo apt install linux-headers-$(uname -r)
```

### rumble_mouse: "Cannot open /dev/uinput"

**Cause:** uinput module not loaded or permissions issue

**Fix:**
```bash
# Load uinput
sudo modprobe uinput

# Check permissions
ls -l /dev/uinput

# Add udev rule for uinput
echo 'KERNEL=="uinput", MODE="0660", GROUP="input"' | \
  sudo tee /etc/udev/rules.d/99-uinput.rules

sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

## Uninstallation

### Quick Uninstall
```bash
cd scripts
sudo ./teardown.sh
```

### Manual Uninstall

1. **Unload module:**
   ```bash
   sudo rmmod rumble
   ```

2. **Remove udev rule:**
   ```bash
   sudo rm /etc/udev/rules.d/99-rumble.rules
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

3. **Clean build artifacts:**
   ```bash
   make clean
   ```

4. **Remove from input group (optional):**
   ```bash
   sudo gpasswd -d $USER input
   ```

---

## Persistent Installation

To load the driver automatically on boot:

### Option 1: Copy to Kernel Modules

```bash
# Copy module
sudo cp driver/rumble.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a

# Load on boot
echo "rumble" | sudo tee /etc/modules-load.d/rumble.conf
```

### Option 2: DKMS (Dynamic Kernel Module Support)

Create `/usr/src/rumble-1.0/dkms.conf`:
```
PACKAGE_NAME="rumble"
PACKAGE_VERSION="1.0"
BUILT_MODULE_NAME[0]="rumble"
DEST_MODULE_LOCATION[0]="/kernel/drivers/usb/misc"
AUTOINSTALL="yes"
```

Then:
```bash
sudo dkms add -m rumble -v 1.0
sudo dkms build -m rumble -v 1.0
sudo dkms install -m rumble -v 1.0
```

---

## Development Setup

For active development:

```bash
# Build and reload quickly
make clean && make && sudo rmmod rumble && sudo insmod driver/rumble.ko

# Watch kernel logs
sudo dmesg -w | grep rumble

# Rebuild tools
cd tools && make clean && make
```

---

## Multiple Controllers

The current driver supports only one controller (minor 0).

To support multiple controllers, modify:
- `RUMBLE_MINOR_COUNT` in `driver/rumble.c`
- Device allocation logic in `rumble_probe()`
- Global `g_dev` to an array

This is left as an exercise for advanced users.

---

## Security Considerations

### Why sudo?

The tools require sudo because:
- `/dev/rumble0` is owned by root:input
- `/dev/uinput` requires special permissions

### Running without sudo

Add your user to the input group:
```bash
sudo usermod -aG input $USER
```

For uinput access:
```bash
echo 'KERNEL=="uinput", MODE="0660", GROUP="input"' | \
  sudo tee /etc/udev/rules.d/99-uinput.rules
sudo udevadm control --reload-rules
```

Then log out and back in.

---

## Next Steps

After installation:

1. **Read the documentation:**
   - `README.md` - User guide
   - `ARCHITECTURE.md` - Technical details
   - `tools/README.md` - Tool documentation

2. **Try the tools:**
   - Start with `rumble_read` for basic testing
   - Use `rumble_monitor` for interactive visualization
   - Try `rumble_mouse` for cursor control

3. **Explore the code:**
   - `driver/rumble.c` - Kernel driver
   - `tools/rumble_monitor.c` - ncurses TUI
   - `tools/rumble_mouse.c` - Mouse mapper

4. **Experiment:**
   - Modify motion parameters in `rumble_mouse.c`
   - Add features to `rumble_monitor.c`
   - Extend the driver for multiple controllers

---

## Support

For issues:
1. Check `dmesg | grep rumble` for kernel messages
2. Verify hardware with `lsusb | grep 045e`
3. Review this troubleshooting guide
4. Check the main README.md

---

**End of Installation Guide**
