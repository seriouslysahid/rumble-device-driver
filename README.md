# rumble — Linux Character Device Driver for Xbox Wireless Controller

> Educational systems project demonstrating USB driver development, character device interfaces, and kernel/userspace interaction.

A Linux kernel module that exposes the Xbox Wireless Controller (Model 1708) through a custom character device interface at `/dev/rumble0`, bypassing the standard Linux Input Subsystem to provide direct access to raw GIP (Gaming Input Protocol) packets.

**Academic Focus:** Linux character drivers, USB interrupt handling, ring-buffered packet delivery, ioctl-based device control, and hotplug safety.

---

## Features

### Kernel Driver
- Custom character device interface (`/dev/rumble0`)
- USB interrupt-IN handling with URBs
- GIP protocol parsing
- 64-slot ring buffer for packet delivery
- Blocking/non-blocking reads
- `poll()` support for event-driven I/O
- `ioctl()` rumble motor control
- kref-based lifetime management
- Hotplug/disconnect handling

### Userspace Tools
- **rumble_read**: Simple CLI packet reader
- **rumble_monitor**: Interactive ncurses TUI with live visualization
- **rumble_mouse**: Controller-to-mouse mapper daemon with smooth motion control

### Automation
- Automated driver binding via `driver_override`
- udev rules for hotplug auto-binding
- Setup/teardown scripts

---

## Repository Structure

```
.
├── driver/          # Kernel module
│   ├── rumble.c     # Driver implementation
│   ├── rumble.h     # Shared ABI header
│   └── Makefile
│
├── tools/           # Userspace tools (C)
│   ├── rumble_read.c      # Basic packet reader
│   ├── rumble_monitor.c   # ncurses TUI
│   ├── Makefile
│   └── README.md
│
├── scripts/         # Automation scripts
│   ├── setup.sh           # Install driver + udev rules
│   ├── teardown.sh        # Uninstall
│   ├── bind.sh            # Manual binding
│   ├── unbind.sh          # Restore xpad
│   ├── 99-rumble.rules    # udev rule
│   └── README.md
│
└── README.md
```

---

## Hardware Target

**Xbox Wireless Controller Model 1708** (wired USB)
- Vendor ID: `045e` (Microsoft)
- Product ID: `02dd`
- Protocol: GIP (Gaming Input Protocol)

Verify your controller:
```bash
lsusb | grep 045e
```

Expected output:
```
Bus 001 Device 003: ID 045e:02dd Microsoft Corp. Xbox One Controller
```

---

## Quick Start

### 1. Build and Install

```bash
# One-command setup (builds + installs)
sudo ./setup.sh
```

This will:
- Check dependencies
- Build kernel module and tools
- Load the rumble driver
- Install udev rules for auto-binding
- Configure `/dev/rumble0` permissions

### 2. Add User to Input Group

```bash
sudo usermod -aG input $USER
# Log out and back in
```

### 3. Test

Plug in your Xbox 1708 controller, then:

```bash
# Interactive monitor
cd tools
sudo ./rumble_monitor

# Simple packet reader
sudo ./rumble_read

# Controller-to-mouse mapper
sudo ./rumble_mouse
```

---

## Usage

### rumble_read
Continuously reads and prints controller packets.

```bash
sudo ./rumble_read
```

**Controls:**
- `Enter` — test rumble (50% both motors, 500ms)
- `Ctrl+C` — exit

### rumble_monitor
Interactive ncurses TUI with live visualization.

```bash
sudo ./rumble_monitor
```

**Controls:**
- `q` — quit
- `r` — test rumble (50% both motors, 500ms)
- `space` — fire current rumble setting
- `↑/↓` — adjust left motor intensity
- `←/→` — adjust right motor intensity

**Display:**
- Live stick visualization (9x9 grid)
- Trigger bars
- Button state
- Packet rate monitoring

### rumble_mouse
Controller-to-mouse mapper daemon with smooth motion control.

```bash
sudo ./rumble_mouse
```

**Controls:**
- Left stick → cursor movement
- Right stick → scrolling
- LB → left click
- RB → right click
- LS click → middle click
- Ctrl+C → exit

**Features:**
- Smooth, responsive cursor control with acceleration
- Radial deadzones (no stick drift)
- Velocity-based motion (frame-rate independent)
- Exponential smoothing filter (reduces jitter)
- Fixed 125 Hz update rate
- Works with Wayland and X11

See `tools/MOUSE_MAPPER.md` for technical details.

---

## Architecture

### Kernel Space

```
Xbox Controller (USB)
       │
       ├─ Interrupt IN (EP1)  ─→  URB completion handler
       │                           (interrupt context)
       │                                  │
       │                                  ▼
       │                           Ring buffer (64 slots)
       │                           spinlock protected
       │                                  │
       │                                  ▼
       │                           wake_up_interruptible()
       │                                  │
       ├─ Interrupt OUT (EP1) ◀─  ioctl(RUMBLE_SET_MOTORS)
       │                           (process context)
       │
       ▼
  /dev/rumble0
  (character device)
       │
       ├─ open()   → kref_get()
       ├─ read()   → ring_get() + copy_to_user()
       ├─ poll()   → poll_wait()
       ├─ ioctl()  → usb_interrupt_msg()
       └─ release() → kref_put()
```

### User Space

```
/dev/rumble0
       │
       ├─ rumble_read    (simple reader)
       ├─ rumble_monitor (ncurses TUI)
       └─ rumble_mouse   (controller-to-mouse mapper)
```

### Driver Binding

```
Controller plugged in
       │
       ▼
udev detects (045e:02dd)
       │
       ▼
Set driver_override="rumble"
       │
       ▼
Unbind from xpad (if bound)
       │
       ▼
Bind to rumble driver
       │
       ▼
/dev/rumble0 created
```

---

## ABI

The driver exposes a stable binary interface through `struct rumble_input`:

```c
struct rumble_input {
    uint16_t buttons;       /* button bitmask */
    uint8_t  lt;            /* left trigger 0-255 */
    uint8_t  rt;            /* right trigger 0-255 */
    int16_t  lx, ly;        /* left stick X,Y */
    int16_t  rx, ry;        /* right stick X,Y */
    uint16_t _pad;          /* alignment padding */
    uint64_t timestamp_us;  /* kernel timestamp */
} __attribute__((packed));  /* 22 bytes total */
```

**Button Masks:**
```c
RUMBLE_BTN_A, RUMBLE_BTN_B, RUMBLE_BTN_X, RUMBLE_BTN_Y
RUMBLE_BTN_LB, RUMBLE_BTN_RB, RUMBLE_BTN_LS, RUMBLE_BTN_RS
RUMBLE_BTN_MENU, RUMBLE_BTN_VIEW
RUMBLE_BTN_DPAD_UP, RUMBLE_BTN_DPAD_DOWN
RUMBLE_BTN_DPAD_LEFT, RUMBLE_BTN_DPAD_RIGHT
```

**Rumble Control:**
```c
struct rumble_motors {
    uint8_t left;   /* 0-100% */
    uint8_t right;  /* 0-100% */
};

ioctl(fd, RUMBLE_SET_MOTORS, &motors);
```

---

## Build Requirements

### Kernel Module
- Linux kernel 6.4+ headers
- GCC
- Make

**Ubuntu/Debian:**
```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

### Userspace Tools
- GCC
- Make
- ncurses development headers (for rumble_monitor)

**Ubuntu/Debian:**
```bash
sudo apt install gcc make libncurses-dev
```

---

## Debugging

### Kernel Logs
```bash
# View rumble driver messages
dmesg | grep rumble

# Live tail
sudo dmesg -w | grep rumble
```

### USB Traffic
```bash
# Load usbmon
sudo modprobe usbmon

# Find bus number
lsusb | grep 045e

# Capture (replace '1' with actual bus number)
sudo cat /sys/kernel/debug/usb/usbmon/1u
```

### Device Status
```bash
# Check device node
ls -l /dev/rumble0

# Check driver binding
ls -l /sys/bus/usb/drivers/rumble/

# Check module status
lsmod | grep rumble
```

---

## Uninstall

```bash
cd scripts
sudo ./teardown.sh
```

This will:
- Unload the rumble kernel module
- Remove udev rules
- Restore xpad driver binding

---

## Design Principles

This project intentionally prioritizes:
- **Clarity** over abstraction
- **Educational value** over production completeness
- **Direct kernel interaction** over framework layers
- **Minimal dependencies** over feature richness
- **Systems programming aesthetics** over GUI polish

The driver deliberately **bypasses the Linux Input Subsystem** to demonstrate:
- Custom character device implementation
- Direct USB interrupt handling
- Ring buffer design
- Kernel/userspace ABI design
- ioctl-based device control

---

## Educational Value

### OS Concepts Demonstrated
- Character device drivers
- USB driver architecture
- Interrupt vs process context
- Ring buffer implementation
- Blocking/non-blocking I/O
- poll() implementation
- ioctl() command interface
- kref reference counting
- Hotplug handling
- sysfs driver binding

### Userspace Concepts
- Character device I/O
- poll() for event-driven I/O
- ioctl() for device control
- ncurses TUI programming
- Binary protocol parsing

---

## Known Limitations

| Limitation | Details |
|------------|---------|
| Wired USB only | Wireless dongle not supported |
| Single controller | Only `/dev/rumble0` (minor 0) |
| Model 1708 only | Other Xbox controllers not tested |
| Basic rumble | Main motors only (no trigger motors) |
| No Input Subsystem | Standard joystick tools won't see it |
| Academic scope | Educational project, not production driver |

---

## Technical Details

### GIP Protocol
The driver implements a subset of Microsoft's Gaming Input Protocol (GIP):
- **Input reports (0x20)**: 18-byte packets with buttons/axes/triggers
- **Virtual-key reports (0x07)**: Guide button with ACK requirement
- **Rumble commands (0x09)**: 13-byte motor control packets

### Ring Buffer
- 64-slot circular buffer (power-of-2 for efficient masking)
- Producer: URB completion handler (interrupt context)
- Consumer: read() syscall (process context)
- Spinlock protection
- Overflow handling: drop oldest packet

### Lifetime Management
- kref-based reference counting
- Safe disconnect while file descriptors open
- Memory freed when last reference drops
- URB killed before teardown

---

## License

GPL-2.0-only

See source files for details.

---

## Team

PathFinders

---

## Tested Environment

- Linux kernel 6.4+
- Ubuntu 22.04 / 24.04
- GCC 11+
- Xbox Wireless Controller Model 1708 (wired USB)

---

## References

- [Linux USB Driver Documentation](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [Linux Device Drivers (LDD3)](https://lwn.net/Kernel/LDD3/)
- [xpad driver source](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)
- [GIP Protocol Analysis](https://github.com/medusalix/xone)
