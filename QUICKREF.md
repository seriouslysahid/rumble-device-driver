# Quick Reference

Fast lookup for common commands and concepts.

---

## Build Commands

```bash
make                    # Build everything
make driver             # Build kernel module only
make tools              # Build userspace tools only
make clean              # Clean all build artifacts
```

---

## Installation

```bash
cd scripts
sudo ./setup.sh         # Install driver + udev rules
sudo ./teardown.sh      # Uninstall everything
```

---

## Manual Binding

```bash
cd scripts
sudo ./bind.sh          # Bind controller to rumble
sudo ./unbind.sh        # Restore xpad binding
```

---

## Running Tools

```bash
cd tools
sudo ./rumble_read      # Simple packet reader
sudo ./rumble_monitor   # ncurses TUI
```

---

## Debugging

```bash
# Check module status
lsmod | grep rumble

# View kernel logs
dmesg | grep rumble
sudo dmesg -w | grep rumble  # Live tail

# Check device node
ls -l /dev/rumble0

# Check USB device
lsusb | grep 045e

# Check driver binding
ls -l /sys/bus/usb/drivers/rumble/

# Check udev rule
cat /etc/udev/rules.d/99-rumble.rules
```

---

## File Locations

| File | Path |
|------|------|
| Kernel module | `driver/rumble.ko` |
| Device node | `/dev/rumble0` |
| Udev rule | `/etc/udev/rules.d/99-rumble.rules` |
| Driver sysfs | `/sys/bus/usb/drivers/rumble/` |
| Device sysfs | `/sys/bus/usb/devices/<busnum>-<devnum>/` |

---

## USB Device IDs

| Field | Value |
|-------|-------|
| Vendor ID | `045e` (Microsoft) |
| Product ID | `02dd` (Xbox 1708) |
| Interface | 0 (GIP interface) |
| Class | `FF` (Vendor-specific) |
| Subclass | `47` |
| Protocol | `D0` |
| EP IN | `0x81` (EP1 IN) |
| EP OUT | `0x01` (EP1 OUT) |

---

## ABI

### struct rumble_input (22 bytes)

```c
uint16_t buttons;       // Offset 0
uint8_t  lt;            // Offset 2
uint8_t  rt;            // Offset 3
int16_t  lx;            // Offset 4
int16_t  ly;            // Offset 6
int16_t  rx;            // Offset 8
int16_t  ry;            // Offset 10
uint16_t _pad;          // Offset 12
uint64_t timestamp_us;  // Offset 14
```

### Button Masks

```c
RUMBLE_BTN_A           (1 << 4)
RUMBLE_BTN_B           (1 << 5)
RUMBLE_BTN_X           (1 << 6)
RUMBLE_BTN_Y           (1 << 7)
RUMBLE_BTN_LB          (1 << 12)
RUMBLE_BTN_RB          (1 << 13)
RUMBLE_BTN_LS          (1 << 2)
RUMBLE_BTN_RS          (1 << 3)
RUMBLE_BTN_MENU        (1 << 0)
RUMBLE_BTN_VIEW        (1 << 1)
RUMBLE_BTN_DPAD_UP     (1 << 8)
RUMBLE_BTN_DPAD_DOWN   (1 << 9)
RUMBLE_BTN_DPAD_LEFT   (1 << 10)
RUMBLE_BTN_DPAD_RIGHT  (1 << 11)
```

### ioctl

```c
struct rumble_motors {
    uint8_t left;   // 0-100%
    uint8_t right;  // 0-100%
};

ioctl(fd, RUMBLE_SET_MOTORS, &motors);
```

---

## GIP Protocol

### Input Report (Type 0x20)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 1 | Type (0x20) |
| 1 | 1 | Sequence |
| 2 | 1 | Flags |
| 3 | 1 | Length (0x0E) |
| 4-5 | 2 | Buttons |
| 6-9 | 4 | Triggers (10-bit) |
| 10-17 | 8 | Axes (4 × int16_t) |

### Rumble Command (Type 0x09)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 1 | Type (0x09) |
| 1 | 1 | Sequence |
| 2 | 1 | Subcommand (0x00) |
| 3 | 1 | Motor mask (0x09) |
| 4-5 | 2 | Trigger motors |
| 6-7 | 2 | Main motors (0-255) |
| 8-10 | 3 | Duration/delay/repeat |
| 11-12 | 2 | Padding |

---

## Key Functions

### Driver

| Function | Purpose |
|----------|---------|
| `rumble_probe()` | USB device initialization |
| `rumble_disconnect()` | USB device removal |
| `rumble_open()` | Character device open |
| `rumble_read()` | Character device read |
| `rumble_poll()` | Character device poll |
| `rumble_ioctl()` | Character device ioctl |
| `rumble_release()` | Character device close |
| `rumble_urb_complete()` | URB completion callback |
| `ring_put()` | Add packet to ring buffer |
| `ring_get()` | Remove packet from ring buffer |

### Synchronization

| Primitive | Purpose | Context |
|-----------|---------|---------|
| `ring_lock` | Protect ring buffer | Interrupt + Process |
| `tx_mutex` | Serialize rumble TX | Process only |
| `read_wq` | Block readers | Process only |
| `disconnected` | Signal removal | Atomic |
| `kref` | Reference counting | Any |

---

## Common Issues

### "No such device"
- Module not loaded: `sudo insmod driver/rumble.ko`
- Controller not plugged in: check `lsusb`
- Wrong PID: verify 045e:02dd

### "Permission denied"
- Not in input group: `sudo usermod -aG input $USER` (then logout/login)
- Or use sudo: `sudo ./rumble_read`

### "Device or resource busy"
- Already bound to xpad: `sudo ./scripts/bind.sh`
- Another process has device open: close it first

### "Cannot allocate memory"
- Out of kernel memory: reboot or free memory
- Check dmesg for details

---

## Performance

| Metric | Value |
|--------|-------|
| Packet rate | 125 Hz (8ms) |
| Packet size | 22 bytes |
| Ring buffer | 64 slots (~512ms) |
| Latency | ~8-9ms (USB-limited) |
| CPU usage | <1% |

---

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | User guide |
| `ARCHITECTURE.md` | Technical details |
| `SUMMARY.md` | Project overview |
| `CHECKLIST.md` | Pre-demo checklist |
| `QUICKREF.md` | This file |
| `tools/README.md` | Tool documentation |
| `scripts/README.md` | Script documentation |

---

## Useful Links

- [Linux USB API](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [LDD3](https://lwn.net/Kernel/LDD3/)
- [xpad driver](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)
- [GIP protocol](https://github.com/medusalix/xone/blob/master/docs/gip.md)

---

**End of Quick Reference**
