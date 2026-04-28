"""
store.py — UI-thread-owned state store.

All mutation happens on the UI thread via apply(event).
No locks needed — only the UI thread writes.
"""

import time
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

from .abi import Sample

# ── History sizes ─────────────────────────────────────────────────────────────

TIMING_HISTORY  = 200   # inter-packet intervals (ms)
TRACE_HISTORY   = 120   # stick position trace points
CIRC_HISTORY    = 1000  # circularity test points
DEBUG_HISTORY   = 16    # raw packet hex lines


@dataclass
class State:
    # Connection
    connected:    bool            = False
    device_path:  str             = "/dev/rumble0"
    last_error:   str             = ""
    connect_time: float           = 0.0   # monotonic
    disconnect_time: float        = 0.0

    # Latest sample
    sample: Optional[Sample]      = None
    prev_recv_ns: int             = 0

    # Packet timing
    intervals_ms: deque           = field(default_factory=lambda: deque(maxlen=TIMING_HISTORY))
    packet_count: int             = 0
    rate_hz:      float           = 0.0
    mean_ms:      float           = 0.0
    jitter_ms:    float           = 0.0

    # Stick traces  [(x_norm, y_norm), ...]
    ltrace: deque                 = field(default_factory=lambda: deque(maxlen=TRACE_HISTORY))
    rtrace: deque                 = field(default_factory=lambda: deque(maxlen=TRACE_HISTORY))

    # Circularity test
    circ_active:  bool            = False
    circ_points:  deque           = field(default_factory=lambda: deque(maxlen=CIRC_HISTORY))
    circ_stick:   str             = "left"   # "left" | "right"

    # Debug
    debug_lines:  deque           = field(default_factory=lambda: deque(maxlen=DEBUG_HISTORY))

    # Rumble
    rumble_left:  int             = 0
    rumble_right: int             = 0
    rumble_status: str            = ""

    # ── derived ──────────────────────────────────────────────────────────────

    @property
    def idle_s(self) -> float:
        if self.sample is None:
            return 0.0
        return (time.monotonic_ns() - self.sample.recv_monotonic_ns) / 1e9

    @property
    def disconnect_duration_s(self) -> float:
        if self.connected or self.disconnect_time == 0.0:
            return 0.0
        return time.monotonic() - self.disconnect_time

    # ── mutation ─────────────────────────────────────────────────────────────

    def apply(self, event_type: str, payload) -> None:
        match event_type:
            case "connected":
                self.connected    = True
                self.connect_time = time.monotonic()
                self.last_error   = ""
                self.device_path  = payload or self.device_path

            case "disconnected":
                self.connected       = False
                self.disconnect_time = time.monotonic()
                self.sample          = None
                self.prev_recv_ns    = 0
                self.intervals_ms.clear()
                self.rate_hz  = 0.0
                self.mean_ms  = 0.0
                self.jitter_ms = 0.0

            case "error":
                self.last_error = str(payload)

            case "sample":
                s: Sample = payload
                self._update_timing(s)
                self._update_traces(s)
                self._update_debug(s)
                self.sample = s

            case "hotplug_add" | "hotplug_remove":
                pass   # reader loop handles reconnect

    def _update_timing(self, s: Sample) -> None:
        self.packet_count += 1
        if self.prev_recv_ns:
            dt_ms = (s.recv_monotonic_ns - self.prev_recv_ns) / 1e6
            self.intervals_ms.append(dt_ms)
            if len(self.intervals_ms) >= 2:
                vals = list(self.intervals_ms)
                self.mean_ms   = sum(vals) / len(vals)
                self.rate_hz   = 1000.0 / self.mean_ms if self.mean_ms > 0 else 0.0
                variance       = sum((v - self.mean_ms) ** 2 for v in vals) / len(vals)
                self.jitter_ms = variance ** 0.5
        self.prev_recv_ns = s.recv_monotonic_ns

    def _update_traces(self, s: Sample) -> None:
        self.ltrace.append((s.lx_n, s.ly_n))
        self.rtrace.append((s.rx_n, s.ry_n))
        if self.circ_active:
            pt = (s.lx_n, s.ly_n) if self.circ_stick == "left" else (s.rx_n, s.ry_n)
            self.circ_points.append(pt)

    def _update_debug(self, s: Sample) -> None:
        hex_str = s.raw.hex(" ").upper()
        self.debug_lines.append(hex_str)
