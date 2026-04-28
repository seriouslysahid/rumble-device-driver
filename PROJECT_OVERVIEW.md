# Project Overview

**Rumble - Xbox Controller Character Device Driver**  
Educational Linux kernel module for third-year operating systems coursework

---

## What Is This?

A Linux character device driver that exposes Xbox Wireless Controller (Model 1708) input through `/dev/rumble0`, bypassing the standard Input Subsystem to demonstrate:

- Character device implementation
- USB interrupt handling
- Ring buffer design
- Kernel/userspace ABI
- Driver binding mechanisms
- Systems programming in C

---

## Repository Structure

```
rumble-device-driver/
├── setup.sh                 # One-command installation
├── Makefile                 # Top-level build
│
├── driver/                  # Kernel module (~800 lines)
│   ├── rumble.c            # Driver implementation
│   ├── rumble.h            # Shared ABI header
│   └── Makefile            # Kbuild makefile
│
├── tools/                   # Userspace tools (~900 lines)
│   ├── rumble_read.c       # Simple packet reader
│   ├── rumble_monitor.c    # ncurses TUI
│   ├── rumble_mouse.c      # Controller-to-mouse mapper
│   ├── MOUSE_MAPPER.md     # Mouse mapper technical docs
│   ├── README.md           # Tool documentation
│   └── Makefile            # Tool build
│
├── scripts/                 # Automation
│   ├── setup.sh            # Install driver + udev
│   ├── teardown.sh         # Uninstall
│   ├── bind.sh             # Manual binding
│   ├── unbind.sh           # Restore xpad
│   ├── 99-rumble.rules     # Udev rule
│   └── README.md           # Script documentation
│
└── docs/                    # Documentation
    ├── README.md           # User guide
    ├── ARCHITECTURE.md     # Technical deep-dive
    ├── SUMMARY.md          # Project summary
    ├── INSTALL.md          # Installation guide
    ├── CHECKLIST.md        # Pre-demo checklist
    ├── QUICKREF.md         # Quick reference
    └── SANITY_CHECK_REPORT.md  # Final review
```

**Total:** ~1700 lines of C code

---

## Quick Start

```bash
# Install (builds + loads driver + configures udev)
sudo ./setup.sh

# Add user to input group
sudo usermod -aG input $USER
# (log out and back in)

# Test
cd tools
sudo ./rumble_monitor    # Interactive TUI
sudo ./rumble_read       # Simple reader
sudo ./rumble_mouse      # Mouse mapper
```

---

## Key Features

### Kernel Driver
- Custom character device (`/dev/rumble0`)
- USB interrupt-IN handling with URBs
- 64-slot ring buffer (power-of-2)
- Blocking/non-blocking reads
- poll() support
- ioctl() rumble control
- kref-based lifetime management
- Safe hotplug/disconnect handling

### Userspace Tools
- **rumble_read**: CLI packet reader with rumble testing
- **rumble_monitor**: ncurses TUI with live visualization
- **rumble_mouse**: Controller-to-mouse mapper with:
  - Smooth motion control (velocity-based)
  - Radial deadzones (no stick drift)
  - Exponential smoothing (reduces jitter)
  - Acceleration curves (precision + speed zones)
  - uinput integration (Wayland/X11 compatible)

### Automation
- One-command setup script
- Automated driver binding via driver_override
- Udev rules for hotplug auto-binding
- Clean teardown script

---

## Educational Value

### OS Concepts Demonstrated

**Character Devices:**
- File operations (open, read, poll, ioctl, release)
- Device node creation
- Major/minor number allocation

**USB Drivers:**
- URB handling
- Interrupt transfers
- Endpoint discovery
- Hotplug/disconnect handling

**Synchronization:**
- Spinlocks (interrupt context)
- Mutexes (process context)
- Wait queues (blocking I/O)
- Atomic variables

**Memory Management:**
- kref reference counting
- DMA-coherent buffers
- Ring buffer implementation

**Userspace Integration:**
- Binary ABI design
- copy_to_user/copy_from_user
- ioctl interface
- poll() implementation
- uinput virtual device creation

---

## Technical Highlights

### Ring Buffer
- 64-slot circular buffer
- Producer: URB callback (interrupt context)
- Consumer: read() syscall (process context)
- Spinlock protection
- Overflow handling: drop oldest

### GIP Protocol
- Input reports (0x20): buttons/axes/triggers
- Virtual-key reports (0x07): Guide button with ACK
- Rumble commands (0x09): motor control

### Mouse Mapper
- epoll + timerfd event loop (125 Hz)
- Radial deadzone with smooth scaling
- Exponential moving average filter (α=0.7)
- Velocity-based motion with piecewise acceleration
- Fractional pixel accumulation
- uinput virtual mouse device

---

## Design Decisions

### Why Bypass Input Subsystem?

**Educational:** Demonstrates custom character device without framework abstraction  
**Technical:** Direct access to raw GIP packets, custom ABI design  
**Practical:** Simpler to explain in viva/demo

### Why C-Only Userspace?

**Before:** Python GUI, Python mouse mapper, ROS2 integration  
**After:** C CLI reader, C ncurses TUI, C mouse mapper

**Reasons:** Consistent language, no heavyweight dependencies, better educational value, easier to build

### Why driver_override?

**Modern:** Kernel 4.0+ feature  
**Clean:** Reversible, no global blacklisting  
**Educational:** Demonstrates sysfs driver binding

---

## Hardware Target

**Xbox Wireless Controller Model 1708** (wired USB)
- Vendor ID: `045e` (Microsoft)
- Product ID: `02dd`
- Protocol: GIP (Gaming Input Protocol)

Verify: `lsusb | grep 045e:02dd`

---

## Build Requirements

- Linux kernel 6.4+ with headers
- GCC compiler
- Make
- ncurses development headers

**Ubuntu/Debian:**
```bash
sudo apt install build-essential linux-headers-$(uname -r) libncurses-dev
```

---

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | User guide and quick start |
| `ARCHITECTURE.md` | Technical deep-dive |
| `SUMMARY.md` | Project summary for viva |
| `INSTALL.md` | Complete installation guide |
| `CHECKLIST.md` | Pre-demo checklist |
| `QUICKREF.md` | Quick command reference |
| `SANITY_CHECK_REPORT.md` | Final code review |

---

## Demo Workflow

1. **Show Architecture** - Explain character device vs Input Subsystem
2. **Build and Install** - Run `sudo ./setup.sh`
3. **Show Driver Binding** - Explain driver_override mechanism
4. **Run Monitor** - Demonstrate live visualization
5. **Demo Mouse Mapper** - Show cursor control (optional)
6. **Show Code** - Walk through key functions
7. **Explain Concepts** - Interrupt vs process context, ring buffer, kref

---

## Testing Status

✓ Functional tests passed  
✓ Stability tests passed (long-running)  
✓ Hotplug handling verified  
✓ Memory leak checks clean  
✓ Code review complete  
✓ Documentation complete  

**Status: READY FOR DEMO/VIVA**

---

## Known Limitations (By Design)

- Wired USB only (wireless donkey not supported)
- Single controller (minor 0 only)
- Model 1708 only (other models not tested)
- Basic rumble (trigger motors not exposed)
- No Input Subsystem integration (by design)

These are intentional for an educational project.

---

## Team

PathFinders

---

## License

GPL-2.0-only

---

## Key Takeaways

1. Character devices provide clean kernel/userspace interface
2. USB interrupt handling requires careful context management
3. Ring buffers efficiently bridge interrupt and process contexts
4. Reference counting (kref) prevents use-after-free bugs
5. Driver binding can be automated with sysfs and udev
6. Simplicity beats complexity for educational projects

---

**End of Project Overview**
