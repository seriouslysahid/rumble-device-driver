# rumble-dash

DearPyGui dashboard for `/dev/rumble0` — live visualisation of the Xbox Wireless
Controller (Model 1708) via the custom `rumble` kernel driver.

## Prerequisites

```bash
# Python 3.11+
pip install dearpygui

# Optional: faster hotplug detection
pip install pyudev
```

## Load the driver

```bash
cd driver && make
sudo insmod driver/rumble.ko
```

## Grant access to /dev/rumble0

```bash
# Persistent (recommended)
echo 'SUBSYSTEM=="rumble", KERNEL=="rumble0", MODE="0660", GROUP="input"' \
  | sudo tee /etc/udev/rules.d/99-rumble.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG input $USER   # log out and back in

# One-shot (testing only)
sudo chmod a+rw /dev/rumble0
```

## Run

```bash
cd rumble_dash
python -m rumble_dash              # uses /dev/rumble0
python -m rumble_dash /dev/rumble0 # explicit device path
```

Or install and use the console script:

```bash
pip install -e .
rumble-dash
```

## Panels

| Panel | Description |
|-------|-------------|
| Status strip | Connected/disconnected state, device path, packet rate, idle time, last error |
| Sticks | Left and right analog stick circles with deadzone, fading trace, raw values |
| Triggers | LT / RT vertical bars with numeric values |
| Buttons | Xbox-style grid: face, D-pad, bumpers, system, stick clicks |
| Timing | Packet rate, mean interval, jitter, inter-packet timing plot |
| Rumble | Left/right motor sliders, Send / Pulse / Stop buttons |
| Debug | Collapsible hex dump of recent raw packets |
| Circularity | Record stick rotation trace vs reference circle; coverage metric |

## Architecture

```
reader thread  ──→  bounded queue (256)  ──→  UI thread
hotplug thread ──→  (same queue)

UI thread:
  drain queue → state.apply() → render panels → dpg.render_frame()
```

- Reader thread: `os.open` + `select.poll` + fixed-size reads, daemon
- State: single `State` object, mutated only on the UI thread
- Rumble: `fcntl.ioctl(fd, RUMBLE_SET_MOTORS, struct.pack("BB", l, r))`
- No SDL, evdev, hidraw, asyncio, Tkinter, or browser stack

## ABI

Mirrors `driver/rumble.h` exactly:

```
struct rumble_input (22 bytes, packed):
  uint16_t buttons      @ 0   bitmask
  uint8_t  lt           @ 2   0–255
  uint8_t  rt           @ 3   0–255
  int16_t  lx           @ 4   signed
  int16_t  ly           @ 6   signed
  int16_t  rx           @ 8   signed
  int16_t  ry           @ 10  signed
  uint16_t _pad         @ 12
  uint64_t timestamp_us @ 14  ktime microseconds

Python format: '<HBBhhhhHQ'

RUMBLE_SET_MOTORS = _IOW('R', 1, 2) = 0x40025201
```
