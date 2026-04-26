# rumble — Linux Character-Device Driver for the Xbox Wireless Controller

> Academic systems project focused on USB driver development, raw controller I/O, and Linux user-space integration.

`rumble` is a Linux kernel module and user-space tooling stack built around the Xbox Wireless Controller (Model 1708) over wired USB. Instead of exposing the controller through the standard Linux Input Subsystem, the project implements a custom character-device interface at `/dev/rumble0`.

The project was developed to explore:
- Linux USB driver architecture
- URB handling and interrupt transfers
- Character-device interfaces
- User/kernel ABI design
- User-space controller tooling
- uinput-based desktop input injection
- ROS 2 integration and robotics teleoperation

The repository includes:
- a Linux kernel driver
- user-space debugging tools
- a DearPyGui-based controller dashboard
- a controller-to-mouse mapper using `uinput`
- a ROS 2 teleoperation demo

The implementation intentionally stays lightweight and educational rather than attempting to become a production input framework.

---

# Features

## Kernel driver
- Custom Linux kernel module (`rumble.ko`)
- USB interrupt-IN handling
- GIP-style Xbox controller packet parsing
- Character device interface (`/dev/rumble0`)
- Blocking/nonblocking reads
- `poll()` support
- `ioctl()`-based rumble control
- Hotplug/disconnect handling
- Ring-buffered packet delivery

## User-space tooling
- Live packet reader (`tools/test_read`)
- DearPyGui controller visualizer
- Linux desktop mouse mapper (`uinput`)
- ROS 2 teleoperation node

## Desktop integration
- Wayland-compatible virtual mouse injection
- X11-compatible virtual mouse injection
- Controller-driven cursor movement and scrolling

---

# Repository Layout

```text
rumble-device-driver/
├── driver/
│   ├── rumble.c
│   ├── rumble.h
│   └── Makefile
│
├── tools/
│   ├── test_read.c
│   └── Makefile
│
├── rumble_dash/
│   ├── pyproject.toml
│   ├── README.md
│   └── rumble_dash/
│       ├── abi.py
│       ├── reader.py
│       ├── store.py
│       ├── hotplug.py
│       └── ui/
│
├── mouse/
│   ├── rumble_mouse.py
│   ├── 60-rumble-uinput.rules
│   └── README.md
│
├── ros2/
│   ├── README.md
│   └── rumble_teleop/
│
└── README.md
```

---

# Hardware Target

The project targets the Xbox Wireless Controller (Model 1708) over wired USB.

The exact USB PID may vary depending on firmware revision and connection mode. The repository currently targets the wired USB configuration used during development and testing.

To verify the controller on your system:

```bash
lsusb | grep 045e
```

Example output:

```text
Bus 001 Device 003: ID 045e:02dd Microsoft Corp. Xbox One Controller
```

---

# Architecture Overview

## Kernel-side

```text
Xbox Controller
       │
 USB Interrupt Transfers
       │
┌───────────────────────┐
│    rumble.ko driver   │
│                       │
│  - URB handling       │
│  - GIP parsing        │
│  - ring buffer        │
│  - ioctl rumble       │
└───────────────────────┘
       │
       ▼
 /dev/rumble0
```

The driver bypasses:

* SDL
* `xboxdrv`
* `evdev`
* the Linux Input Subsystem
* `hidraw`

Applications read structured controller packets directly from `/dev/rumble0`.

---

## User-space

```text
/dev/rumble0
       │
       ├── test_read
       ├── DearPyGui dashboard
       ├── ROS 2 teleop
       └── uinput mouse mapper
```

---

# Build Requirements

## Kernel build dependencies

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    linux-headers-$(uname -r)
```

## User-space dependencies

```bash
sudo apt install -y gcc make python3 python3-pip
```

---

# Building the Kernel Module

```bash
cd driver
make
```

This produces:

```text
rumble.ko
```

To clean:

```bash
make clean
```

---

# Loading the Driver

## Insert module

```bash
sudo insmod driver/rumble.ko
```

## Verify load

```bash
dmesg | grep rumble
```

Example:

```text
[rumble] module loaded
[rumble] controller connected
```

## Remove module

```bash
sudo rmmod rumble
```

---

# Device Permissions

The driver creates:

```text
/dev/rumble0
```

By default this is root-owned.

## Recommended: udev rule

Create:

```text
/etc/udev/rules.d/99-rumble.rules
```

Contents:

```udev
SUBSYSTEM=="rumble", KERNEL=="rumble0", MODE="0660", GROUP="input"
```

Reload rules:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Add your user to the input group:

```bash
sudo usermod -aG input $USER
```

Log out and back in afterward.

---

# User-Space Test Tool

The repository includes a small CLI packet reader.

## Build

```bash
cd tools
make
```

## Run

```bash
./test_read
```

Example output:

```text
BTN=A LT=0 RT=0 LX=0 LY=0 RX=0 RY=0
```

Press:

* `Enter` → test rumble pulse
* `Ctrl+C` → exit

---

# DearPyGui Dashboard

The repository includes a Linux-native controller dashboard implemented with DearPyGui.

Features:

* live button visualization
* analog stick rendering
* trigger visualization
* packet timing metrics
* circularity testing
* rumble controls
* debug packet inspection

## Install dependencies

```bash
cd rumble_dash
pip install -e .
```

## Run

```bash
python -m rumble_dash
```

---

# Mouse Mapper

The repository includes a lightweight controller-to-mouse mapper implemented using Linux `uinput`.

Features:

* left stick → cursor movement
* right stick → scrolling
* LS click → left click
* RS click → right click
* LB → middle click
* LT precision mode
* Wayland support
* X11 support

The mapper reads `/dev/rumble0` and emits a virtual mouse device through `/dev/uinput`.

## Install dependencies

```bash
pip install python-uinput
```

## Load uinput module

```bash
sudo modprobe uinput
```

## Install udev rule

```bash
sudo cp mouse/60-rumble-uinput.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Run

```bash
python mouse/rumble_mouse.py
```

---

# ROS 2 Teleoperation

The repository includes a ROS 2 teleop node for TurtleBot3 simulation.

Features:

* controller-driven robot movement
* stick-based teleoperation
* collision-triggered rumble feedback

See:

```text
ros2/README.md
```

for full setup instructions.

---

# ABI Overview

The driver exposes controller packets through a shared ABI.

Kernel-side structure:

```c
struct rumble_input {
    uint16_t buttons;
    uint8_t  lt;
    uint8_t  rt;
    int16_t  lx, ly, rx, ry;
    uint16_t _pad;
    uint64_t timestamp_us;
} __attribute__((packed));
```

The dashboard, teleop node, and mouse mapper all consume this same ABI.

---

# Debugging

## Kernel logs

```bash
dmesg | grep rumble
```

Live tail:

```bash
sudo dmesg -w
```

---

## USB traffic inspection

```bash
sudo modprobe usbmon
```

Find bus:

```bash
lsusb
```

Capture:

```bash
sudo cat /sys/kernel/debug/usb/usbmon/1u
```

---

## Device visibility

Check created devices:

```bash
ls -l /dev/rumble0
```

Check uinput virtual mouse:

```bash
libinput list-devices
```

or under X11:

```bash
xinput list
```

---

# Known Limitations

| Limitation                              | Details                                                                               |
| --------------------------------------- | ------------------------------------------------------------------------------------- |
| Wired USB only                          | Wireless dongle support is not implemented                                            |
| Single-controller focus                 | The repository is designed around one active controller                               |
| No Linux Input Subsystem integration    | Standard joystick tools will not see the controller                                   |
| No force-feedback subsystem integration | Rumble uses a custom ioctl interface                                                  |
| Experimental protocol handling          | The project targets a specific controller configuration used during development       |
| Academic scope                          | The repository prioritizes clarity and educational value over production completeness |

---

# Design Goals

This project intentionally prioritizes:

* clarity over abstraction
* Linux-native interfaces
* direct USB interaction
* educational readability
* small user-space tools
* practical debugging visibility

The repository intentionally avoids:

* large frameworks
* complex daemon architectures
* plugin systems
* heavyweight remapping stacks
* unnecessary abstraction layers

---

# Tested Environment

The project was primarily tested on:

* Linux kernel 6.4+
* Ubuntu-based distributions
* Wayland and X11 desktop sessions
* Python 3.11+

---

# License

GPL-2.0-only.

See individual source files for details.
