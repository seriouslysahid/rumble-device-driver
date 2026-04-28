"""sticks.py — Analog stick visualisation using DPG draw primitives."""

import math
import dearpygui.dearpygui as dpg
from ...ui.theme import C_STICK_BG, C_STICK_RIM, C_DEADZONE, C_DOT, C_ACCENT, C_DIM

_R      = 70    # outer radius px
_DZ     = 0.12  # deadzone fraction
_SZ     = _R * 2 + 10
_CX     = _SZ // 2
_CY     = _SZ // 2


def _draw_stick(dl: str, trace, nx: float, ny: float, label: str) -> None:
    dpg.delete_item(dl, children_only=True)

    # Background circle
    dpg.draw_circle([_CX, _CY], _R, color=C_STICK_RIM,
                    fill=C_STICK_BG, thickness=1.5, parent=dl)
    # Deadzone circle
    dz_r = int(_R * _DZ)
    dpg.draw_circle([_CX, _CY], dz_r, color=C_DEADZONE,
                    fill=C_DEADZONE, thickness=1, parent=dl)
    # Cross-hair
    dpg.draw_line([_CX - _R, _CY], [_CX + _R, _CY],
                  color=(*C_DIM[:3], 60), thickness=1, parent=dl)
    dpg.draw_line([_CX, _CY - _R], [_CX, _CY + _R],
                  color=(*C_DIM[:3], 60), thickness=1, parent=dl)

    # Trace
    pts = list(trace)
    if len(pts) >= 2:
        for i in range(1, len(pts)):
            alpha = int(60 * i / len(pts))
            x0 = _CX + pts[i-1][0] * _R
            y0 = _CY - pts[i-1][1] * _R
            x1 = _CX + pts[i][0]   * _R
            y1 = _CY - pts[i][1]   * _R
            dpg.draw_line([x0, y0], [x1, y1],
                          color=(*C_ACCENT[:3], alpha), thickness=1, parent=dl)

    # Current position dot
    px = _CX + nx * _R
    py = _CY - ny * _R
    dpg.draw_circle([px, py], 5, color=C_DOT, fill=C_DOT, thickness=0, parent=dl)

    # Label
    dpg.draw_text([4, _SZ - 14], label, color=C_DIM, size=12, parent=dl)


def build() -> dict:
    tags: dict = {}
    with dpg.group(horizontal=True):
        # Left stick
        with dpg.group():
            dpg.add_text("Left Stick", color=C_DIM)
            tags["ldl"] = dpg.add_drawlist(width=_SZ, height=_SZ)
            with dpg.group(horizontal=True):
                dpg.add_text("X:", color=C_DIM)
                dpg.add_text("0", tag="lx_val")
                dpg.add_spacer(width=6)
                dpg.add_text("Y:", color=C_DIM)
                dpg.add_text("0", tag="ly_val")
            tags["lx"] = "lx_val"
            tags["ly"] = "ly_val"

        dpg.add_spacer(width=20)

        # Right stick
        with dpg.group():
            dpg.add_text("Right Stick", color=C_DIM)
            tags["rdl"] = dpg.add_drawlist(width=_SZ, height=_SZ)
            with dpg.group(horizontal=True):
                dpg.add_text("X:", color=C_DIM)
                dpg.add_text("0", tag="rx_val")
                dpg.add_spacer(width=6)
                dpg.add_text("Y:", color=C_DIM)
                dpg.add_text("0", tag="ry_val")
            tags["rx"] = "rx_val"
            tags["ry"] = "ry_val"

    return tags


def update(tags: dict, state) -> None:
    s = state.sample
    lx = s.lx_n if s else 0.0
    ly = s.ly_n if s else 0.0
    rx = s.rx_n if s else 0.0
    ry = s.ry_n if s else 0.0

    _draw_stick(tags["ldl"], state.ltrace, lx, ly, "L")
    _draw_stick(tags["rdl"], state.rtrace, rx, ry, "R")

    dpg.set_value(tags["lx"], f"{s.lx:+6d}" if s else "0")
    dpg.set_value(tags["ly"], f"{s.ly:+6d}" if s else "0")
    dpg.set_value(tags["rx"], f"{s.rx:+6d}" if s else "0")
    dpg.set_value(tags["ry"], f"{s.ry:+6d}" if s else "0")
