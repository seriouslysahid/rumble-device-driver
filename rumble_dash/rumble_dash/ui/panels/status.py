"""status.py — Connection status strip."""

import dearpygui.dearpygui as dpg
from ...ui.theme import C_GREEN, C_RED, C_DIM


def build() -> dict:
    """Build the status strip. Returns tag dict for update()."""
    tags: dict = {}
    with dpg.group(horizontal=True):
        dpg.add_text("●", tag="st_dot")
        tags["dot"] = "st_dot"
        dpg.add_text("DISCONNECTED", tag="st_label")
        tags["label"] = "st_label"
        dpg.add_spacer(width=20)
        dpg.add_text("Device:", color=C_DIM)
        dpg.add_text("/dev/rumble0", tag="st_dev")
        tags["dev"] = "st_dev"
        dpg.add_spacer(width=20)
        dpg.add_text("Rate:", color=C_DIM)
        dpg.add_text("—", tag="st_rate")
        tags["rate"] = "st_rate"
        dpg.add_spacer(width=20)
        dpg.add_text("Idle:", color=C_DIM)
        dpg.add_text("—", tag="st_idle")
        tags["idle"] = "st_idle"
        dpg.add_spacer(width=20)
        dpg.add_text("", tag="st_err", color=C_RED)
        tags["err"] = "st_err"
    return tags


def update(tags: dict, state) -> None:
    if state.connected:
        dpg.set_value(tags["dot"],   "●")
        dpg.configure_item(tags["dot"],   color=C_GREEN)
        dpg.set_value(tags["label"], "CONNECTED")
        dpg.configure_item(tags["label"], color=C_GREEN)
        rate = f"{state.rate_hz:.1f} Hz" if state.rate_hz > 0 else "—"
        dpg.set_value(tags["rate"], rate)
        idle = f"{state.idle_s*1000:.0f} ms" if state.sample else "—"
        dpg.set_value(tags["idle"], idle)
    else:
        dpg.set_value(tags["dot"],   "●")
        dpg.configure_item(tags["dot"],   color=C_RED)
        dpg.set_value(tags["label"], "DISCONNECTED")
        dpg.configure_item(tags["label"], color=C_RED)
        dpg.set_value(tags["rate"], "—")
        dpg.set_value(tags["idle"], "—")

    dpg.set_value(tags["dev"], state.device_path)
    err = state.last_error
    dpg.set_value(tags["err"], err[:60] if err else "")
