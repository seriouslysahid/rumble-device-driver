# Rumble Userspace Tools

Simple C-based tools for interacting with the rumble character device driver.

## Tools

### `rumble_read`
Basic packet reader that prints controller state to stdout.

**Usage:**
```bash
sudo ./rumble_read
```

**Features:**
- Continuous packet reading from `/dev/rumble0`
- Pretty-printed button/axis/trigger values
- Press Enter to test rumble
- Press Ctrl+C to exit

### `rumble_monitor`
Interactive ncurses TUI for live controller monitoring.

**Usage:**
```bash
sudo ./rumble_monitor
```

**Features:**
- Live visual display of sticks, triggers, and buttons
- Packet rate monitoring
- Interactive rumble testing
- Keyboard controls:
  - `q` — quit
  - `r` — test rumble (50% both motors, 500ms)
  - `space` — fire current rumble setting
  - `↑/↓` — adjust left motor intensity
  - `←/→` — adjust right motor intensity

### `rumble_mouse`
Controller-to-mouse mapper daemon with proper motion modeling.

**Usage:**
```bash
sudo ./rumble_mouse
```

**Features:**
- Smooth, responsive cursor control
- Radial deadzones with smooth scaling
- Velocity-based motion with acceleration
- Exponential smoothing filter (reduces jitter)
- Fractional pixel accumulation (no stair-stepping)
- Fixed 125 Hz update rate (frame-rate independent)
- Scrolling support (right stick)
- Button mapping:
  - Left stick → cursor movement
  - Right stick → scrolling (vertical + horizontal)
  - LB → left click
  - RB → right click
  - LS click → middle click

**Technical Details:**
- Uses epoll + timerfd for precise timing
- Implements radial deadzone (~12% radius)
- Applies exponential moving average filter (α=0.7)
- Piecewise acceleration curve (precision + speed zones)
- Integrates velocity over fixed 8ms timesteps
- Emits events via uinput (Wayland/X11 compatible)

**Configuration:**
Edit `rumble_mouse.c` to adjust:
- `DEADZONE_RADIUS` — stick deadzone (default 4000)
- `CURSOR_BASE_SPEED` — base cursor speed (default 800 px/s)
- `CURSOR_ACCEL` — acceleration multiplier (default 1.5)
- `FILTER_ALPHA` — smoothing strength (default 0.7)
- `SCROLL_SCALE` — scroll sensitivity (default 0.3)

## Building

```bash
make              # build all tools
make rumble_read  # build reader only
make rumble_monitor  # build monitor only
make rumble_mouse    # build mouse mapper only
make clean        # remove binaries
```

## Dependencies

- `rumble_read`: none (standard C library only)
- `rumble_monitor`: ncurses (`libncurses-dev` on Debian/Ubuntu)
- `rumble_mouse`: libm (math library, usually included)

## Notes

- All tools require the rumble kernel module to be loaded
- All tools require read/write access to `/dev/rumble0`
- `rumble_mouse` additionally requires write access to `/dev/uinput`
- Use `sudo` or add your user to the `input` group (see main README)

## uinput Permissions

For `rumble_mouse` to work without sudo:

```bash
# Load uinput module
sudo modprobe uinput

# Add udev rule for uinput access
echo 'KERNEL=="uinput", MODE="0660", GROUP="input"' | \
  sudo tee /etc/udev/rules.d/99-uinput.rules

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Add your user to input group
sudo usermod -aG input $USER
# Log out and back in
```
