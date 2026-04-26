"""
abi.py — Python mirror of driver/rumble.h

struct rumble_input layout (packed, 22 bytes):
  uint16_t buttons      @ 0
  uint8_t  lt           @ 2
  uint8_t  rt           @ 3
  int16_t  lx           @ 4
  int16_t  ly           @ 6
  int16_t  rx           @ 8
  int16_t  ry           @ 10
  uint16_t _pad         @ 12
  uint64_t timestamp_us @ 14
  total                 = 22 bytes

struct rumble_motors (2 bytes):
  uint8_t left
  uint8_t right

RUMBLE_SET_MOTORS = _IOW('R', 1, struct rumble_motors)
  = 0x40000000 | (2 << 16) | (ord('R') << 8) | 1
  = 0x40025201
"""

import struct
from dataclasses import dataclass

# ── Wire format ──────────────────────────────────────────────────────────────

INPUT_FMT  = "<HBBhhhhHQ"   # little-endian, packed
INPUT_SIZE = struct.calcsize(INPUT_FMT)
assert INPUT_SIZE == 22, f"ABI mismatch: expected 22 bytes, got {INPUT_SIZE}"

MOTORS_FMT  = "BB"
MOTORS_SIZE = struct.calcsize(MOTORS_FMT)  # 2

# ── ioctl number ─────────────────────────────────────────────────────────────
# _IOW(magic, nr, size) = 0x40000000 | (size << 16) | (magic << 8) | nr
RUMBLE_SET_MOTORS: int = (
    0x40000000
    | (MOTORS_SIZE << 16)
    | (ord("R") << 8)
    | 1
)  # 0x40025201

# ── Button constants (mirror driver/rumble.h) ─────────────────────────────────

BTN_MENU       = 1 << 0
BTN_VIEW       = 1 << 1
BTN_LS         = 1 << 2
BTN_RS         = 1 << 3
BTN_A          = 1 << 4
BTN_B          = 1 << 5
BTN_X          = 1 << 6
BTN_Y          = 1 << 7
BTN_DPAD_UP    = 1 << 8
BTN_DPAD_DOWN  = 1 << 9
BTN_DPAD_LEFT  = 1 << 10
BTN_DPAD_RIGHT = 1 << 11
BTN_LB         = 1 << 12
BTN_RB         = 1 << 13

# Ordered list for UI rendering: (mask, label, group)
BUTTON_DEFS: list[tuple[int, str, str]] = [
    (BTN_A,          "A",     "face"),
    (BTN_B,          "B",     "face"),
    (BTN_X,          "X",     "face"),
    (BTN_Y,          "Y",     "face"),
    (BTN_LB,         "LB",    "shoulder"),
    (BTN_RB,         "RB",    "shoulder"),
    (BTN_MENU,       "MENU",  "system"),
    (BTN_VIEW,       "VIEW",  "system"),
    (BTN_LS,         "LS",    "stick"),
    (BTN_RS,         "RS",    "stick"),
    (BTN_DPAD_UP,    "↑",     "dpad"),
    (BTN_DPAD_DOWN,  "↓",     "dpad"),
    (BTN_DPAD_LEFT,  "←",     "dpad"),
    (BTN_DPAD_RIGHT, "→",     "dpad"),
]

# ── Sample dataclass ──────────────────────────────────────────────────────────

@dataclass(slots=True, frozen=True)
class Sample:
    buttons:          int
    lt:               int    # 0-255
    rt:               int    # 0-255
    lx:               int    # signed 16-bit
    ly:               int    # signed 16-bit
    rx:               int    # signed 16-bit
    ry:               int    # signed 16-bit
    timestamp_us:     int    # kernel ktime microseconds
    recv_monotonic_ns: int   # time.monotonic_ns() at read
    raw:              bytes

    # Normalise axes to [-1.0, 1.0]
    @property
    def lx_n(self) -> float: return self.lx / 32768.0
    @property
    def ly_n(self) -> float: return self.ly / 32768.0
    @property
    def rx_n(self) -> float: return self.rx / 32768.0
    @property
    def ry_n(self) -> float: return self.ry / 32768.0
    @property
    def lt_n(self) -> float: return self.lt / 255.0
    @property
    def rt_n(self) -> float: return self.rt / 255.0

    def btn(self, mask: int) -> bool:
        return bool(self.buttons & mask)


def parse(raw: bytes, recv_ns: int) -> Sample:
    """Parse raw bytes into a Sample. Raises ValueError on size mismatch."""
    if len(raw) != INPUT_SIZE:
        raise ValueError(f"Expected {INPUT_SIZE} bytes, got {len(raw)}")
    buttons, lt, rt, lx, ly, rx, ry, _pad, ts = struct.unpack(INPUT_FMT, raw)
    return Sample(
        buttons=buttons, lt=lt, rt=rt,
        lx=lx, ly=ly, rx=rx, ry=ry,
        timestamp_us=ts,
        recv_monotonic_ns=recv_ns,
        raw=raw,
    )
