# rumble_mouse — Virtual Mouse Mapper

A lightweight, single-process controller-to-mouse mapper for the `rumble` driver.

It reads the raw gamepad state from `/dev/rumble0` and creates a virtual mouse using `/dev/uinput` to inject relative cursor movements and mouse button clicks.

## Why uinput?

Using `/dev/uinput` avoids compositor-specific hacks and X11 dependencies (like XTEST). The virtual device looks exactly like a standard hardware mouse to the Linux input stack, ensuring perfect compatibility natively across both **Wayland** (GNOME, KDE, Sway) and **X11**.

## Requirements

- Python 3.11+
- `python-uinput` package (`pip install python-uinput`)
- `uinput` kernel module loaded (`sudo modprobe uinput`)
- Proper permissions to `/dev/uinput`

## Setup

1. Copy the udev rule to grant access to the uinput device:
   ```bash
   sudo cp 60-rumble-uinput.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```
2. Make sure your user is in the `input` group:
   ```bash
   sudo usermod -aG input $USER
   ```
   *(You may need to log out and log back in for group changes to take effect).*

## Running

```bash
python3 rumble_mouse.py
```

## Mappings

| Controller | Action |
| --- | --- |
| **Left Stick** | Move cursor |
| **Right Stick Y** | Scroll vertically |
| **Right Stick X** | Scroll horizontally |
| **Left Stick Click** | Left click (BTN_LEFT) |
| **Right Stick Click** | Right click (BTN_RIGHT) |
| **Left Bumper (LB)** | Middle click (BTN_MIDDLE) |
| **Left Trigger (LT) > 50%**| Precision mode (reduces sensitivity) |

## Tuning

All tuning constants (deadzone, scaling speeds, update rate, precision multiplier) are neatly contained in the `Config` dataclass at the top of `rumble_mouse.py`. Feel free to edit them to adjust the sensitivity or update rate to your liking.

## Known Limitations

- The mapper connects strictly to `/dev/rumble0`. If you have multiple controllers, only the first is tracked.
- It requires the controller to stay plugged in; there is no background daemon auto-reconnect loop. If it disconnects, the mapper exits cleanly.
