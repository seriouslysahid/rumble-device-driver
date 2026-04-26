#!/usr/bin/env python3
"""
rumble_mouse.py — Lightweight controller-to-mouse mapper for Linux.
"""

import math
import os
import select
import signal
import struct
import sys
import time
from dataclasses import dataclass

try:
    import uinput
except ImportError:
    print("Error: python-uinput is required. (pip install python-uinput)")
    sys.exit(1)

# ---------------------------------------------------------------------------
# ABI & Constants
# ---------------------------------------------------------------------------

INPUT_FMT = "<HBBhhhhHQ"
INPUT_SIZE = struct.calcsize(INPUT_FMT)
if INPUT_SIZE != 22:
    print(f"ABI mismatch: expected 22 bytes, got {INPUT_SIZE}")
    sys.exit(1)

BTN_LS = 1 << 2
BTN_RS = 1 << 3
BTN_LB = 1 << 12

@dataclass
class Config:
    dev_path: str = "/dev/rumble0"
    hz: int = 250
    deadzone: int = 4000
    max_axis: float = 32768.0
    
    speed_x: float = 24.0       # pixels per tick at max deflection
    speed_y: float = 24.0
    scroll_y: float = 1.0       # scroll clicks per tick at max deflection
    scroll_x: float = 1.0
    
    precision_mult: float = 0.25 # LT > 50% multiplier

# ---------------------------------------------------------------------------
# Mapper
# ---------------------------------------------------------------------------

def apply_deadzone(val: int, deadzone: int, max_val: float) -> float:
    if abs(val) < deadzone:
        return 0.0
    sign = 1.0 if val > 0 else -1.0
    magnitude = abs(val)
    normalized = (magnitude - deadzone) / (max_val - deadzone)
    return sign * min(1.0, normalized)

class RumbleMouse:
    def __init__(self):
        self.config = Config()
        self.fd = -1
        self.uinput_dev = None
        self.running = False
        
        self.last_buttons = 0
        
        self.residue_x = 0.0
        self.residue_y = 0.0
        self.residue_scroll_y = 0.0
        self.residue_scroll_x = 0.0

    def init_uinput(self):
        events = (
            uinput.REL_X,
            uinput.REL_Y,
            uinput.REL_WHEEL,
            uinput.REL_HWHEEL,
            uinput.BTN_LEFT,
            uinput.BTN_RIGHT,
            uinput.BTN_MIDDLE,
        )
        try:
            self.uinput_dev = uinput.Device(events, name="Rumble Virtual Mouse")
        except OSError as e:
            print(f"Error creating uinput device: {e}")
            print("Ensure the 'uinput' kernel module is loaded and you have permissions to /dev/uinput.")
            sys.exit(1)
            
        time.sleep(0.05)

    def open_device(self):
        try:
            self.fd = os.open(self.config.dev_path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as e:
            print(f"Error opening {self.config.dev_path}: {e}")
            print("Is the rumble kernel driver loaded and accessible?")
            sys.exit(1)

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1
        if self.uinput_dev:
            if self.last_buttons & BTN_LS:
                self.uinput_dev.emit(uinput.BTN_LEFT, 0, syn=False)
            if self.last_buttons & BTN_RS:
                self.uinput_dev.emit(uinput.BTN_RIGHT, 0, syn=False)
            if self.last_buttons & BTN_LB:
                self.uinput_dev.emit(uinput.BTN_MIDDLE, 0, syn=False)
            
            if self.last_buttons:
                self.uinput_dev.syn()
                
            self.uinput_dev.destroy()
            self.uinput_dev = None

    def tick(self, raw_data: bytes):
        buttons, lt, rt, lx, ly, rx, ry, _pad, ts = struct.unpack(INPUT_FMT, raw_data)
        
        mult = self.config.precision_mult if lt > 127 else 1.0
        
        nx = apply_deadzone(lx, self.config.deadzone, self.config.max_axis)
        ny = apply_deadzone(ly, self.config.deadzone, self.config.max_axis)
        
        dx = nx * self.config.speed_x * mult + self.residue_x
        dy = -ny * self.config.speed_y * mult + self.residue_y
        
        move_x = int(dx)
        move_y = int(dy)
        self.residue_x = dx - move_x
        self.residue_y = dy - move_y
        
        sx = apply_deadzone(rx, self.config.deadzone, self.config.max_axis)
        sy = apply_deadzone(ry, self.config.deadzone, self.config.max_axis)
        
        dsy = sy * self.config.scroll_y * mult + self.residue_scroll_y
        dsx = sx * self.config.scroll_x * mult + self.residue_scroll_x
        
        scroll_y = int(dsy)
        scroll_x = int(dsx)
        self.residue_scroll_y = dsy - scroll_y
        self.residue_scroll_x = dsx - scroll_x

        emitted = False
        if move_x != 0:
            self.uinput_dev.emit(uinput.REL_X, move_x, syn=False)
            emitted = True
        if move_y != 0:
            self.uinput_dev.emit(uinput.REL_Y, move_y, syn=False)
            emitted = True
        if scroll_y != 0:
            self.uinput_dev.emit(uinput.REL_WHEEL, scroll_y, syn=False)
            emitted = True
        if scroll_x != 0:
            self.uinput_dev.emit(uinput.REL_HWHEEL, scroll_x, syn=False)
            emitted = True

        changed = buttons ^ self.last_buttons
        if changed & BTN_LS:
            self.uinput_dev.emit(uinput.BTN_LEFT, 1 if (buttons & BTN_LS) else 0, syn=False)
            emitted = True
        if changed & BTN_RS:
            self.uinput_dev.emit(uinput.BTN_RIGHT, 1 if (buttons & BTN_RS) else 0, syn=False)
            emitted = True
        if changed & BTN_LB:
            self.uinput_dev.emit(uinput.BTN_MIDDLE, 1 if (buttons & BTN_LB) else 0, syn=False)
            emitted = True
            
        if emitted:
            self.uinput_dev.syn()
            
        self.last_buttons = buttons

    def run(self):
        self.open_device()
        self.init_uinput()
        
        self.running = True
        
        interval = 1.0 / self.config.hz
        next_tick = time.monotonic() + interval
        
        current_sample = None
        
        print(f"Rumble Mouse Mapper running at {self.config.hz} Hz.")
        print("Press Ctrl-C to exit.")
        
        while self.running:
            now = time.monotonic()
            timeout = max(0.0, next_tick - now)
            
            try:
                r, _, _ = select.select([self.fd], [], [], timeout)
            except InterruptedError:
                break
                
            if r:
                while True:
                    try:
                        chunk = os.read(self.fd, INPUT_SIZE)
                        if not chunk:
                            print("\nController disconnected.")
                            self.running = False
                            break
                        if len(chunk) == INPUT_SIZE:
                            current_sample = chunk
                    except BlockingIOError:
                        break
                    except OSError:
                        print("\nController disconnected (OSError).")
                        self.running = False
                        break
            
            now = time.monotonic()
            if now >= next_tick:
                if current_sample and self.running:
                    self.tick(current_sample)
                    
                next_tick += interval
                if now > next_tick + interval:
                    next_tick = now + interval

def main():
    mapper = RumbleMouse()
    
    def handle_sigint(signum, frame):
        mapper.running = False
        
    signal.signal(signal.SIGINT, handle_sigint)
    signal.signal(signal.SIGTERM, handle_sigint)
    
    try:
        mapper.run()
    finally:
        mapper.close()

if __name__ == "__main__":
    main()
