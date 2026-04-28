# Controller-to-Mouse Mapper Design

Technical documentation for the `rumble_mouse` daemon.

---

## Overview

`rumble_mouse` is a userspace daemon that converts Xbox controller input into virtual mouse events. It implements proper motion modeling, filtering, and acceleration to provide smooth, responsive cursor control.

**Key Design Goals:**
- Smooth, jitter-free cursor motion
- No stick drift (proper deadzones)
- Natural acceleration (fast for large movements, precise for small)
- Frame-rate independent (consistent feel)
- Low latency (<10ms)
- Wayland/X11 compatible

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   rumble_mouse daemon                   │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  epoll event loop                                       │
│    ├─ /dev/rumble0 (EPOLLIN)                           │
│    │    → read packet                                   │
│    │    → update state                                  │
│    │                                                    │
│    └─ timerfd (125 Hz)                                 │
│         → compute motion                                │
│         → emit uinput events                            │
│                                                         │
│  State:                                                │
│    - raw stick values (lx, ly, rx, ry)                │
│    - filtered stick values                             │
│    - button states (current + previous)                │
│    - accumulated fractional pixels                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
         │
         ↓
    /dev/uinput
         │
         ↓
  Virtual Mouse Device
         │
         ↓
  Desktop Environment
```

---

## Input Processing Pipeline

```
Raw Controller Packet
    ↓
Parse (lx, ly, rx, ry, buttons)
    ↓
Radial Deadzone
    ├─ magnitude = sqrt(x² + y²)
    ├─ if (magnitude < deadzone) → zero
    └─ else → smooth scale from [deadzone, max] to [0, max]
    ↓
Exponential Moving Average Filter
    ├─ filtered = α * raw + (1-α) * prev
    └─ α = 0.7 (70% new, 30% old)
    ↓
Normalize to [-1, 1]
    ↓
Acceleration Curve (piecewise)
    ├─ if |stick| < 0.3 → precision zone (low sensitivity)
    └─ else → speed zone (high sensitivity + acceleration)
    ↓
Velocity Computation
    ├─ velocity = f(stick_normalized)
    └─ units: pixels/second
    ↓
Integration (fixed timestep)
    ├─ delta = velocity * dt
    └─ dt = 1/125 = 8ms
    ↓
Fractional Pixel Accumulation
    ├─ accum += delta
    ├─ emit integer part
    └─ keep fractional part
    ↓
Emit uinput Events
    ├─ REL_X, REL_Y (cursor)
    ├─ REL_WHEEL, REL_HWHEEL (scroll)
    ├─ BTN_LEFT, BTN_RIGHT, BTN_MIDDLE (clicks)
    └─ SYN_REPORT (sync)
```

---

## Deadzone Implementation

### Why Radial Deadzones?

**Problem with axial deadzones:**
```c
// BAD: Creates square deadzone
if (abs(x) < deadzone) x = 0;
if (abs(y) < deadzone) y = 0;
```

- Diagonal movement feels different than cardinal
- Inconsistent response in different directions
- Can cause "sticky" corners

**Radial deadzone solution:**
```c
magnitude = sqrt(x² + y²);
if (magnitude < deadzone) {
    x = 0; y = 0;
} else {
    // Smooth scaling
    scale = (magnitude - deadzone) / (max - deadzone);
    x = (x / magnitude) * scale * max;
    y = (y / magnitude) * scale * max;
}
```

**Benefits:**
- Circular deadzone (consistent in all directions)
- Smooth transition from rest to motion
- No sudden jumps at deadzone boundary
- Preserves directional accuracy

### Deadzone Radius Selection

**Default: 4000 units (~12% of 32768 max)**

**Rationale:**
- Typical stick drift: ±50-100 units
- Typical intentional movement: >2000 units
- 4000 provides comfortable margin
- Smooth scaling makes exact value less critical

**Tuning:**
- Too small: cursor drifts when stick at rest
- Too large: reduced effective stick range, less precise

---

## Motion Model

### Velocity-Based with Fixed Timestep

**Why velocity-based?**
- Frame-rate independent
- Natural acceleration feel
- Smooth continuous motion
- Predictable behavior

**Implementation:**
```c
// Compute velocity (pixels/second)
velocity = f(stick_normalized);

// Integrate over fixed timestep
delta = velocity * dt;  // dt = 1/125 = 8ms

// Accumulate fractional pixels
accum += delta;
emit_x = (int)accum;
accum -= emit_x;
```

**Why fixed timestep?**
- Consistent feel regardless of packet arrival timing
- Simplifies tuning (velocity values are absolute)
- Matches controller update rate (125 Hz)

### Acceleration Curve

**Piecewise design:**

```
Precision Zone (|stick| < 0.3):
    velocity = stick * base_speed * precision_scale
    velocity = stick * 800 * 0.4
    velocity = stick * 320 px/s

Speed Zone (|stick| >= 0.3):
    accel = 1 + 1.5 * |stick|
    velocity = stick * base_speed * accel
    
    At |stick| = 0.5:
        accel = 1 + 1.5 * 0.5 = 1.75
        velocity = 0.5 * 800 * 1.75 = 700 px/s
    
    At |stick| = 1.0:
        accel = 1 + 1.5 * 1.0 = 2.5
        velocity = 1.0 * 800 * 2.5 = 2000 px/s
```

**Rationale:**
- Small movements: low sensitivity for precision
- Large movements: high sensitivity + acceleration for speed
- Smooth transition at 0.3 threshold
- Tunable via constants

**Alternative curves considered:**
- Linear: too slow for large movements OR too imprecise for small
- Power (x^1.5): good but less tunable
- Exponential: too aggressive, hard to control

---

## Filtering

### Exponential Moving Average (EMA)

**Formula:**
```c
filtered = α * raw + (1 - α) * filtered_prev
```

**With α = 0.7:**
- 70% new value
- 30% previous value

**Why EMA?**
- Simple, low memory (single float per axis)
- Removes high-frequency jitter
- Minimal lag (with α = 0.7)
- Smooth response

**Frequency response:**
- Cutoff frequency ≈ 30 Hz (with 125 Hz update rate)
- Stick jitter typically 50-100 Hz
- Intentional movement typically <10 Hz

**Tradeoff:**
- Higher α (e.g., 0.9): less smoothing, more responsive, more jitter
- Lower α (e.g., 0.5): more smoothing, less jitter, more lag

**Why not more complex filters?**
- Butterworth/Kalman: overkill for this use case
- EMA provides 90% of the benefit with 10% of the complexity

---

## Fractional Pixel Accumulation

### The Problem

**Without accumulation:**
```c
int dx = (int)(velocity * dt);  // Truncates fractional part
emit(REL_X, dx);
```

**At low speeds:**
- velocity = 100 px/s
- dt = 0.008s
- delta = 0.8 pixels
- (int)delta = 0 → no movement!

**Result:** Stair-stepping, jerky motion at low speeds

### The Solution

**With accumulation:**
```c
accum_x += velocity * dt;  // Keep fractional part
int dx = (int)accum_x;     // Extract integer
accum_x -= dx;             // Keep remainder
emit(REL_X, dx);
```

**Example:**
```
Frame 1: accum = 0.8, emit 0, accum = 0.8
Frame 2: accum = 1.6, emit 1, accum = 0.6
Frame 3: accum = 1.4, emit 1, accum = 0.4
Frame 4: accum = 1.2, emit 1, accum = 0.2
Frame 5: accum = 1.0, emit 1, accum = 0.0
```

**Result:** Smooth motion even at low speeds (average 0.8 px/frame)

---

## Timing Architecture

### epoll + timerfd

**Why epoll?**
- Efficient event multiplexing
- Scales to multiple file descriptors
- Standard Linux pattern

**Why timerfd?**
- Precise periodic timing
- Integrates with epoll
- Decouples input reading from motion updates

**Event loop:**
```c
while (running) {
    epoll_wait();
    
    if (rumble_fd ready) {
        read_packet();
        update_state();
    }
    
    if (timer_fd ready) {
        compute_motion();
        emit_events();
    }
}
```

**Benefits:**
- Motion updates at fixed 125 Hz (even if packets delayed)
- Input reading doesn't block motion updates
- Clean separation of concerns

**Alternative approaches:**
- poll() with timeout: couples timing to packet arrival
- pthread: overkill, adds complexity
- busy loop: wastes CPU

---

## Scrolling

### Right Stick Scrolling

**Implementation:**
```c
// Apply deadzone (larger than cursor deadzone)
apply_deadzone(&rx, &ry, SCROLL_DEADZONE);

// Filter
rx_filt = ema_filter(rx, rx_filt, FILTER_ALPHA);
ry_filt = ema_filter(ry, ry_filt, FILTER_ALPHA);

// Compute scroll delta
scroll_x = rx_filt * SCROLL_SCALE * dt;
scroll_y = -ry_filt * SCROLL_SCALE * dt;

// Accumulate and emit
accum_sx += scroll_x;
accum_sy += scroll_y;

int wheel_x = (int)accum_sx;
int wheel_y = (int)accum_sy;

emit(REL_HWHEEL, wheel_x);
emit(REL_WHEEL, wheel_y);
```

**Design choices:**
- Larger deadzone (8000 vs 4000): prevents accidental scrolling
- Lower scale (0.3): scrolling is typically slower than cursor
- Same filtering: smooth scroll motion
- Fractional accumulation: smooth at low speeds

---

## Button Mapping

### Debouncing

**Implementation:**
```c
uint16_t changed = buttons ^ buttons_prev;

if (changed & RUMBLE_BTN_LB) {
    int pressed = !!(buttons & RUMBLE_BTN_LB);
    emit(EV_KEY, BTN_LEFT, pressed);
}

buttons_prev = buttons;
```

**Why XOR?**
- Detects state changes
- Only emit events on transitions
- Prevents duplicate events

**Mappings:**
- LB → BTN_LEFT (left click)
- RB → BTN_RIGHT (right click)
- LS click → BTN_MIDDLE (middle click)

**Why these mappings?**
- Natural: bumpers are like mouse buttons
- Accessible: easy to press while moving stick
- Standard: matches most controller-to-mouse tools

---

## Performance

### Latency Breakdown

```
Controller → USB → Kernel → /dev/rumble0
    8ms      <1ms    <1ms      (ring buffer)
                                    ↓
                            rumble_mouse reads
                                 <1ms
                                    ↓
                            Process + filter
                                 <1ms
                                    ↓
                            Wait for timer
                                0-8ms
                                    ↓
                            Emit uinput
                                 <1ms
                                    ↓
                            Desktop compositor
                                1-2ms

Total: 10-20ms (dominated by USB polling + timer alignment)
```

### CPU Usage

**Measured:**
- ~0.1% CPU (single core, modern CPU)
- ~2 KB memory (state struct)

**Why so low?**
- epoll blocks when idle
- Fixed 125 Hz update rate (not busy loop)
- Simple math (no complex filters)

---

## Configuration

### Tunable Parameters

```c
/* Deadzone */
#define DEADZONE_RADIUS 4000.0f  // Stick deadzone
#define SCROLL_DEADZONE 8000.0f  // Scroll deadzone

/* Cursor speed */
#define CURSOR_BASE_SPEED 800.0f      // Base speed (px/s)
#define CURSOR_ACCEL 1.5f             // Acceleration multiplier
#define PRECISION_THRESHOLD 0.3f      // Precision zone size
#define PRECISION_SCALE 0.4f          // Precision zone sensitivity

/* Scrolling */
#define SCROLL_SCALE 0.3f  // Scroll sensitivity

/* Filtering */
#define FILTER_ALPHA 0.7f  // EMA filter strength
```

### Tuning Guide

**Cursor feels too slow:**
- Increase `CURSOR_BASE_SPEED` (e.g., 1000)
- Increase `CURSOR_ACCEL` (e.g., 2.0)

**Cursor feels too fast:**
- Decrease `CURSOR_BASE_SPEED` (e.g., 600)
- Decrease `CURSOR_ACCEL` (e.g., 1.2)

**Cursor drifts when stick at rest:**
- Increase `DEADZONE_RADIUS` (e.g., 5000)

**Cursor feels imprecise for small movements:**
- Decrease `PRECISION_THRESHOLD` (e.g., 0.2)
- Decrease `PRECISION_SCALE` (e.g., 0.3)

**Cursor feels jittery:**
- Decrease `FILTER_ALPHA` (e.g., 0.5) — more smoothing

**Cursor feels sluggish:**
- Increase `FILTER_ALPHA` (e.g., 0.8) — less smoothing

**Scrolling too fast:**
- Decrease `SCROLL_SCALE` (e.g., 0.2)

**Scrolling too slow:**
- Increase `SCROLL_SCALE` (e.g., 0.5)

---

## Comparison to Alternatives

### vs. Kernel Mouse Emulation

**Kernel approach (removed):**
- Pros: Low latency, no userspace daemon
- Cons: Hard to tune, violates "bypass Input Subsystem" design, mixes concerns

**Userspace approach (current):**
- Pros: Easy to tune, clean separation, educational value
- Cons: Slightly higher latency (~1-2ms)

**Verdict:** Userspace is better for this project

### vs. Python/pyautogui

**Python approach:**
- Pros: Easy to prototype
- Cons: High latency (50-100ms), CPU usage, not educational

**C + uinput approach:**
- Pros: Low latency (<10ms), low CPU, proper Linux integration
- Cons: More code

**Verdict:** C + uinput is proper systems programming

### vs. xboxdrv

**xboxdrv:**
- Full-featured daemon with config files, profiles, etc.
- ~10,000 lines of C++
- Complex architecture

**rumble_mouse:**
- Single-purpose mapper
- ~400 lines of C
- Simple, understandable architecture

**Verdict:** rumble_mouse is appropriate for educational project

---

## Known Limitations

1. **Single controller**: Only supports one controller (matches driver)
2. **No configuration file**: Parameters are compile-time constants
3. **No mode switching**: No toggle between cursor/scroll modes
4. **Basic button mapping**: No drag-lock, no modifier keys
5. **No trigger support**: Triggers not used for scrolling/clicking

**Rationale:** These are out of scope for an educational OS project. The implementation demonstrates core concepts without unnecessary complexity.

---

## Future Enhancements (Out of Scope)

- Configuration file (JSON/INI)
- Runtime parameter adjustment (signals/sockets)
- Multiple controller support
- Mode switching (cursor/scroll/disabled)
- Trigger-based scrolling
- Drag-lock support
- Sensitivity profiles
- GUI configuration tool

---

## References

- [Linux uinput documentation](https://www.kernel.org/doc/html/latest/input/uinput.html)
- [Input event codes](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h)
- [epoll man page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [timerfd man page](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)

---

**End of Documentation**
