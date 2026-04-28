"""rumble.py — Rumble motor control panel."""

import fcntl
import os
import struct
import threading
import time
import dearpygui.dearpygui as dpg
from ...ui.theme import C_DIM, C_GREEN, C_RED
from ...abi import RUMBLE_SET_MOTORS, MOTORS_FMT

_DEVICE = "/dev/rumble0"
_PULSE_DURATION = 0.5   # seconds


def _send_motors(left: int, right: int) -> str:
    """Send ioctl. Returns "" on success or error string."""
    try:
        # O_RDWR required: ioctl writes to the device
        fd = os.open(_DEVICE, os.O_RDWR)
        payload = struct.pack(MOTORS_FMT, left, right)
        fcntl.ioctl(fd, RUMBLE_SET_MOTORS, payload)
        os.close(fd)
        return ""
    except OSError as exc:
        return str(exc)


def build(state) -> dict:
    tags: dict = {"state": state}
    dpg.add_text("Rumble", color=C_DIM)
    with dpg.group(horizontal=True):
        dpg.add_text("Left:", color=C_DIM)
        tags["left"] = dpg.add_slider_int(
            label="", min_value=0, max_value=100,
            default_value=0, width=120, tag="rm_left")
        dpg.add_spacer(width=8)
        dpg.add_text("Right:", color=C_DIM)
        tags["right"] = dpg.add_slider_int(
            label="", min_value=0, max_value=100,
            default_value=0, width=120, tag="rm_right")

    with dpg.group(horizontal=True):
        dpg.add_button(label="Send", width=70,
                       callback=lambda: _on_send(tags))
        dpg.add_button(label="Pulse", width=70,
                       callback=lambda: _on_pulse(tags))
        dpg.add_button(label="Stop", width=70,
                       callback=lambda: _on_stop(tags))

    dpg.add_text("", tag="rm_status"); tags["status"] = "rm_status"
    return tags


def _on_send(tags: dict) -> None:
    l = dpg.get_value(tags["left"])
    r = dpg.get_value(tags["right"])
    err = _send_motors(l, r)
    _set_status(tags, err)


def _on_stop(tags: dict) -> None:
    err = _send_motors(0, 0)
    _set_status(tags, err or "Stopped")


def _on_pulse(tags: dict) -> None:
    l = dpg.get_value(tags["left"])
    r = dpg.get_value(tags["right"])
    if l == 0 and r == 0:
        l, r = 50, 50

    # Worker only does blocking I/O; result stored in a list for UI thread to pick up.
    # We use a simple flag: pulse_result[0] is set by the thread.
    pulse_result: list[str] = [""]

    def _pulse():
        err = _send_motors(l, r)
        if err:
            pulse_result[0] = f"ERR: {err}"
            return
        time.sleep(_PULSE_DURATION)
        _send_motors(0, 0)
        pulse_result[0] = "Pulse done"

    t = threading.Thread(target=_pulse, daemon=True)
    t.start()
    _set_status(tags, f"Pulsing {l}%/{r}%…")
    # Store thread + result so update() can poll it
    tags["_pulse_thread"] = t
    tags["_pulse_result"] = pulse_result


def _set_status(tags: dict, msg: str) -> None:
    color = C_RED if msg.startswith("ERR") or "error" in msg.lower() else C_GREEN
    dpg.set_value(tags["status"], msg[:60])
    dpg.configure_item(tags["status"], color=color)


def update(tags: dict, state) -> None:
    # Poll pulse thread result on the UI thread (safe DPG call)
    t = tags.get("_pulse_thread")
    if t is not None and not t.is_alive():
        result = tags.get("_pulse_result", [""])[0]
        if result:
            _set_status(tags, result)
        tags.pop("_pulse_thread", None)
        tags.pop("_pulse_result", None)
