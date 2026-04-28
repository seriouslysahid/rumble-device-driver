# Project Summary

## Rumble - Xbox Controller Character Device Driver

**Academic Systems Project** | **Team: PathFinders** | **Linux Kernel 6.4+**

---

## What Is This?

A Linux kernel module that exposes the Xbox Wireless Controller (Model 1708) through a custom character device interface (`/dev/rumble0`), bypassing the standard Linux Input Subsystem to provide direct access to raw GIP (Gaming Input Protocol) packets.

**Core Concept:** Demonstrate character device driver implementation, USB interrupt handling, and kernel/userspace interaction without relying on existing input frameworks.

---

## Key Features

### Kernel Driver
✓ Custom character device (`/dev/rumble0`)  
✓ USB interrupt-IN handling with URBs  
✓ 64-slot ring buffer for packet delivery  
✓ Blocking/non-blocking reads  
✓ `poll()` support  
✓ `ioctl()` rumble control  
✓ kref-based lifetime management  
✓ Hotplug safety  

### Userspace Tools
✓ `rumble_read` - CLI packet reader  
✓ `rumble_monitor` - ncurses TUI with live visualization  
✓ `rumble_mouse` - controller-to-mouse mapper daemon  

### Automation
✓ Automated driver binding via `driver_override`  
✓ udev rules for hotplug auto-binding  
✓ Setup/teardown scripts  

---

## Repository Structure

```
rumble-device-driver/
├── driver/          # Kernel module (~800 lines C)
├── tools/           # Userspace tools (~900 lines C)
├── scripts/         # Automation scripts
├── README.md        # User documentation
├── ARCHITECTURE.md  # Technical deep-dive
└── Makefile         # Top-level build
```

**Total:** ~1700 lines of C code (down from ~2500 lines with Python)

---

## Quick Start

```bash
# Build and install
sudo ./setup.sh

# Add user to input group
sudo usermod -aG input $USER
# (log out and back in)

# Test
cd tools && sudo ./rumble_monitor
```

---

## Educational Value

### OS Concepts Demonstrated

**Character Devices:**
- File operations (`open`, `read`, `poll`, `ioctl`, `release`)
- Device node creation (`/dev/rumble0`)
- Major/minor number allocation

**USB Drivers:**
- URB (USB Request Block) handling
- Interrupt transfers
- Endpoint discovery
- Hotplug/disconnect handling

**Synchronization:**
- Spinlocks (interrupt context)
- Mutexes (process context)
- Wait queues (blocking I/O)
- Atomic variables (disconnect flag)

**Memory Management:**
- kref reference counting
- DMA-coherent buffers
- Ring buffer implementation

**Kernel/Userspace Interface:**
- Binary ABI design
- `copy_to_user()` / `copy_from_user()`
- ioctl command interface
- poll() implementation

**Driver Binding:**
- sysfs `driver_override` mechanism
- udev rules
- USB device matching

---

## Technical Highlights

### Ring Buffer
- 64-slot circular buffer (power-of-2)
- Producer: URB callback (interrupt context)
- Consumer: `read()` syscall (process context)
- Spinlock protection
- Overflow handling: drop oldest

### GIP Protocol
- Input reports (0x20): buttons/axes/triggers
- Virtual-key reports (0x07): Guide button with ACK
- Rumble commands (0x09): motor control

### Lifetime Management
- kref-based reference counting
- Safe disconnect while file descriptors open
- Memory freed when last reference drops

### Context Awareness
- Interrupt context: URB callback (no sleeping)
- Process context: syscalls (can sleep)
- Proper synchronization primitives for each

---

## Design Decisions

### Why Bypass Input Subsystem?

**Educational Reasons:**
- Demonstrates custom character device implementation
- Shows direct USB handling
- Avoids hiding complexity behind frameworks
- Provides clear kernel/userspace boundary

**Technical Reasons:**
- Direct access to raw GIP packets
- Custom ABI design
- Simpler for demonstration purposes
- Easier to explain in viva/demo

### Why C-Only Userspace?

**Before:** Python GUI (DearPyGui), Python mouse mapper, ROS2 integration  
**After:** C CLI reader + C ncurses TUI

**Reasons:**
- Consistent language (all C)
- No heavyweight dependencies
- Better educational value (shows actual syscalls)
- Easier to build and demo
- More "systems programming" aesthetic

### Why driver_override?

**Alternatives Considered:**
- Blacklist xpad globally (too invasive)
- Manual sysfs manipulation (not automated)
- Modify xpad driver (out of scope)

**Chosen Approach:**
- Modern (kernel 4.0+)
- Clean and reversible
- Demonstrates sysfs driver binding
- Good educational value

---

## Simplification Results

### Before Simplification
- ~2500 lines total
- Python GUI (DearPyGui)
- Python mouse mapper
- ROS2 integration
- Kernel mouse emulation
- Fragmented architecture

### After Simplification
- ~1700 lines total
- C-only userspace
- ncurses TUI
- C-based mouse mapper (uinput)
- Focused on core OS concepts
- Clean, cohesive architecture

**Removed:**
- 500+ lines Python GUI
- 200+ lines Python mouse mapper
- 300+ lines ROS2 integration
- 100+ lines kernel mouse emulation

**Added:**
- 250 lines ncurses monitor
- 400 lines C mouse mapper
- Automated binding scripts
- Comprehensive documentation

---

## Demo Workflow

### For Viva/Presentation

1. **Show Architecture**
   - Explain character device vs Input Subsystem
   - Draw kernel/userspace boundary
   - Explain USB interrupt flow

2. **Build and Install**
   ```bash
   make
   cd scripts && sudo ./setup.sh
   ```

3. **Show Driver Binding**
   - Explain `driver_override` mechanism
   - Show sysfs paths
   - Demonstrate udev auto-binding

4. **Run Monitor**
   ```bash
   cd tools && sudo ./rumble_monitor
   ```
   - Show live stick visualization
   - Test rumble
   - Explain poll() usage

5. **Demo Mouse Mapper** (optional)
   ```bash
   cd tools && sudo ./rumble_mouse
   ```
   - Show cursor control
   - Explain uinput integration
   - Demonstrate smooth motion

6. **Show Code**
   - `driver/rumble.c`: URB callback, ring buffer
   - `driver/rumble.h`: ABI definition
   - `tools/rumble_monitor.c`: ncurses + poll()
   - `tools/rumble_mouse.c`: uinput + motion control

7. **Explain Key Concepts**
   - Interrupt vs process context
   - Ring buffer design
   - kref lifetime management
   - GIP protocol parsing

---

## Testing

### Functional Tests
✓ Open/close stress test  
✓ Concurrent readers  
✓ Hotplug during read  
✓ Rumble during disconnect  
✓ Non-blocking I/O  
✓ poll() readiness  

### Stability Tests
✓ Long-running (hours)  
✓ Packet loss measurement  
✓ Memory leak check (kref)  

### Protocol Tests
✓ GIP input parsing  
✓ GIP rumble commands  
✓ Virtual-key ACK  

---

## Known Limitations

| Limitation | Reason |
|------------|--------|
| Wired USB only | Wireless dongle requires different protocol |
| Single controller | Academic scope (minor 0 only) |
| Model 1708 only | Other models not tested |
| Basic rumble | Trigger motors not exposed |
| No Input Subsystem | By design (educational) |

---

## Documentation

- **README.md** - User guide, quick start, usage
- **ARCHITECTURE.md** - Technical deep-dive, data flow, memory layout
- **tools/README.md** - Userspace tool documentation
- **scripts/README.md** - Automation script documentation
- **SUMMARY.md** - This file (project overview)

---

## Build Requirements

**Kernel Module:**
- Linux kernel 6.4+ headers
- GCC
- Make

**Userspace Tools:**
- GCC
- Make
- ncurses development headers

**Ubuntu/Debian:**
```bash
sudo apt install build-essential linux-headers-$(uname -r) libncurses-dev
```

---

## Tested Environment

- **OS:** Ubuntu 22.04 / 24.04
- **Kernel:** 6.4+
- **Compiler:** GCC 11+
- **Hardware:** Xbox Wireless Controller Model 1708 (wired USB)

---

## License

GPL-2.0-only

---

## Team

PathFinders

---

## Academic Context

This project was developed as a third-year operating systems assignment to explore:
- Linux character device drivers
- USB driver architecture
- Kernel/userspace interaction
- Interrupt vs process context
- Synchronization primitives
- Memory management
- Driver binding mechanisms

The implementation prioritizes:
- **Educational clarity** over production completeness
- **Direct kernel interaction** over framework abstraction
- **Simplicity** over feature richness
- **Systems programming aesthetics** over GUI polish

---

## Key Takeaways

1. **Character devices** provide a clean kernel/userspace interface
2. **USB interrupt handling** requires careful context management
3. **Ring buffers** efficiently bridge interrupt and process contexts
4. **Reference counting** (kref) prevents use-after-free bugs
5. **Driver binding** can be automated with sysfs and udev
6. **Simplicity** beats complexity for educational projects

---

## References

- [Linux USB Driver Documentation](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [Linux Device Drivers (LDD3)](https://lwn.net/Kernel/LDD3/)
- [xpad driver source](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)
- [GIP Protocol Analysis](https://github.com/medusalix/xone)

---

**End of Summary**
