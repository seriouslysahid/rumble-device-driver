"""debug.py — Collapsible raw packet hex dump panel."""

import dearpygui.dearpygui as dpg
from ...ui.theme import C_DIM, C_ACCENT, C_TEXT


def build() -> dict:
    tags: dict = {}
    with dpg.collapsing_header(label="Debug / Raw Packets", default_open=False):
        with dpg.group(horizontal=True):
            dpg.add_text("Seq:", color=C_DIM)
            dpg.add_text("0", tag="dbg_seq"); tags["seq"] = "dbg_seq"
            dpg.add_spacer(width=10)
            dpg.add_text("ts_us:", color=C_DIM)
            dpg.add_text("—", tag="dbg_ts"); tags["ts"] = "dbg_ts"

        dpg.add_text("Last packets (hex):", color=C_DIM)
        tags["lines"] = []
        for i in range(8):
            t = dpg.add_text("", tag=f"dbg_line_{i}", color=C_ACCENT)
            tags["lines"].append(t)

    return tags


def update(tags: dict, state) -> None:
    s = state.sample
    dpg.set_value(tags["seq"], str(state.packet_count))
    if s:
        dpg.set_value(tags["ts"], str(s.timestamp_us))

    lines = list(state.debug_lines)[-8:]
    for i, t in enumerate(tags["lines"]):
        dpg.set_value(t, lines[i] if i < len(lines) else "")
