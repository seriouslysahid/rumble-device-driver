# rumble — Xbox Wireless Controller Linux Kernel Driver

> **Team:** PathFinders  
> **Hardware:** Xbox Wireless Controller Model 1708 (USB, VID `0x045E` PID `0x02FD`)  
> **Kernel:** Linux 6.4+

`rumble` is a loadable Linux kernel module that exposes the Xbox Wireless
Controller (Model 1708) as a character device at `/dev/rumble0`.  It bypasses
the Linux Input Subsystem entirely, giving applications direct access to raw
GIP input reports and full control of the rumble motors via `ioctl`.

The wired Model 1708 uses Microsoft's GIP (Gaming Input Protocol), not legacy
HID. This driver parses GIP input reports (type 0x20) and sends GIP rumble
commands.

---

## Project layout

```
rumble/
├── driver/
│   ├── rumble.c          # Kernel module source
│   ├── rumble.h          # Shared structs & ioctl definitions
│   └── Makefile          # Kbuild-compatible Makefile
├── tools/
│   ├── test_read.c       # User-space test program
│   └── Makefile
├── ros2/
│   ├── rumble_teleop/
│   │   ├── rumble_teleop_node.py
│   │   ├── package.xml
│   │   └── setup.py
│   └── README.md
└── README.md             # This file
```

---

## 1. Prerequisites

### Kernel build tools

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
```

### User-space build tools

```bash
sudo apt install -y gcc make
```

### ROS 2 (for the teleop node)

Install ROS 2 Humble or Iron following the official guide:  
https://docs.ros.org/en/humble/Installation.html

### TurtleBot3 packages

```bash
sudo apt install -y ros-humble-turtlebot3 ros-humble-turtlebot3-simulations \
                    ros-humble-gazebo-ros-pkgs
export TURTLEBOT3_MODEL=burger
```

---

## 2. Building the driver

```bash
cd rumble/driver
make
```

This produces `rumble.ko`.  If you need to build against a different kernel
tree (e.g., for cross-compilation):

```bash
make KDIR=/path/to/kernel/source
```

---

## 3. Loading the module

```bash
# Load
sudo insmod driver/rumble.ko

# Verify it loaded
dmesg | grep rumble
# Expected: [rumble] module loaded (major=<N>)
# After plugging in controller:
# [rumble] Xbox 1708 controller connected (bus 1 dev 3)
```

To load automatically at boot, copy `rumble.ko` to the modules directory and
run `depmod`:

```bash
sudo cp driver/rumble.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
echo 'rumble' | sudo tee /etc/modules-load.d/rumble.conf
```

To unload:

```bash
sudo rmmod rumble
```

---

## 4. Permissions — `/dev/rumble0`

After `insmod`, udev creates `/dev/rumble0`.  By default it is owned by
`root:root` with mode `0600`.  Grant access to your user in one of two ways:

### Option A — udev rule (recommended, persistent)

Create `/etc/udev/rules.d/99-rumble.rules`:

```udev
SUBSYSTEM=="rumble", KERNEL=="rumble0", MODE="0660", GROUP="input"
```

Then reload rules and add your user to the `input` group:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG input $USER
# Log out and back in for group change to take effect
```

### Option B — one-shot (for testing only)

```bash
sudo chmod a+rw /dev/rumble0
```

---

## 5. Running the test program

```bash
cd rumble/tools
make
sudo ./test_read           # or just ./test_read if permissions are set
```

### Expected output

```
rumble: opened /dev/rumble0
Press Enter to fire a test rumble.  Ctrl+C to quit.

[12345.678 ms] BTN=(none)              LT=  0 RT=  0  LX=+0    LY=+0     RX=+0    RY=+0
[12353.690 ms] BTN=A                   LT=  0 RT=  0  LX=+0    LY=+0     RX=+0    RY=+0
```

Press **Enter** at any time to fire a 500 ms test rumble at 50% intensity.
Press **Ctrl+C** to exit.

---

## 6. Debugging tools

### Kernel log

```bash
# All rumble messages since module load
dmesg | grep rumble

# Tail in real time
sudo dmesg -w | grep rumble
```

### usbmon — capture raw USB traffic

```bash
# Load the usbmon kernel module
sudo modprobe usbmon

# Find the bus number for your controller
lsusb | grep "045e:02fd"
# e.g. Bus 001 Device 003: ID 045e:02fd Microsoft Corp. Xbox Wireless Controller

# Capture on bus 1 (adjust number to match)
sudo cat /sys/kernel/debug/usb/usbmon/1u | head -200
# Or use Wireshark with the USBPCap / usbmon interface
```

### Hotplug stress test

With `test_read` running, repeatedly unplug and replug the controller:

```bash
# In one terminal
sudo ./tools/test_read

# Unplug/replug the USB cable several times
# Expected: program prints "controller disconnected" then "controller connected"
# after each cycle, and continues reading data without crashing
```

Check `dmesg | grep rumble` for any error messages after each cycle.

---

## 7. ROS 2 demo with TurtleBot3 Gazebo

See [`ros2/README.md`](ros2/README.md) for full instructions.  Quick start:

```bash
# Terminal 1 — simulation
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py

# Terminal 2 — teleop node
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
sudo chmod a+rw /dev/rumble0   # or use udev rule
ros2 run rumble_teleop rumble_teleop_node
```

Drive with the left stick.  Collisions trigger a 300 ms haptic pulse.

---

## 8. Known limitations

| Limitation | Details |
|-----------|---------|
| **Wired USB only** | The driver uses the USB HID interface. Xbox wireless dongles use a proprietary RF protocol not covered here. |
| **Single controller** | Only one controller (minor 0) is supported at a time. A second controller is rejected with `EBUSY`. |
| **Basic rumble only** | The driver exposes left and right motor intensity. The Xbox 1708 also has trigger rumble motors (LT/RT), which this driver does not expose. |
| **No force-feedback API** | The driver deliberately bypasses the Linux FF (force feedback) subsystem. Use the `RUMBLE_SET_MOTORS` ioctl directly. |
| **Model 1708 only** | Only VID `0x045E` PID `0x02FD` is in the ID table. Other Xbox controller variants have different product IDs and may use different report formats. |
| **No kernel Input events** | Because the Input Subsystem is bypassed, standard tools like `evtest`, `jstest`, and `xboxdrv` will not see this controller while the module is loaded. |

---

## Licence

GPL-2.0-only — see individual source files.
