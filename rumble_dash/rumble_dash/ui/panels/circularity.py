"""circularity.py — Stick circularity test panel."""

import math
import dearpygui.dearpygui as dpg
from ...ui.theme import C_DIM, C_CIRC_REF, C_CIRC_PT, C_ACCENT, C_GREEN, C_AMBER

_SZ = 160
_CX = _SZ // 2
_CY = _SZ // 2
_R  = (_SZ // 2) - 8


def _draw(dl: str, points, active: bool) -> None:
    dpg.delete_item(dl, children_only=True)

    # Reference circle
    dpg.draw_circle([_CX, _CY], _R, color=C_CIRC_REF,
                    fill=(0,0,0,0), thickness=1.5, parent=dl)
    # Axes
    dpg.draw_line([_CX - _R, _CY], [_CX + _R, _CY],
                  color=(*C_DIM[:3], 40), thickness=1, parent=dl)
    dpg.draw_line([_CX, _CY - _R], [_CX, _CY + _R],
                  color=(*C_DIM[:3], 40), thickness=1, parent=dl)

    # Recorded points
    pts = list(points)
    for nx, ny in pts:
        px = _CX + nx * _R
        py = _CY - ny * _R
        dpg.draw_circle([px, py], 2, color=C_CIRC_PT,
                        fill=C_CIRC_PT, thickness=0, parent=dl)

    # Status text
    status = "RECORDING" if active else ("DONE" if pts else "IDLE")
    col = C_GREEN if active else C_DIM
    dpg.draw_text([4, 4], status, color=col, size=11, parent=dl)

    # Coverage metric
    if len(pts) >= 10:
        coverage = _coverage(pts)
        dpg.draw_text([4, _SZ - 16], f"Cov: {coverage:.0f}%",
                      color=C_ACCENT, size=11, parent=dl)


def _coverage(pts: list) -> float:
    """Fraction of 36 angular sectors (10° each) that have at least one point."""
    sectors = set()
    for nx, ny in pts:
        angle = math.degrees(math.atan2(ny, nx)) % 360
        sectors.add(int(angle // 10))
    return len(sectors) / 36 * 100


def build(state) -> dict:
    tags: dict = {"state": state}
    with dpg.collapsing_header(label="Circularity Test", default_open=True):
        with dpg.group(horizontal=True):
            dpg.add_text("Stick:", color=C_DIM)
            tags["stick_combo"] = dpg.add_combo(
                ["left", "right"], default_value="left",
                width=80, tag="circ_stick",
                callback=lambda s, v: setattr(state, "circ_stick", v))

        tags["dl"] = dpg.add_drawlist(width=_SZ, height=_SZ)

        with dpg.group(horizontal=True):
            dpg.add_button(label="Start", width=60,
                           callback=lambda: _start(state))
            dpg.add_button(label="Stop",  width=60,
                           callback=lambda: _stop(state))
            dpg.add_button(label="Reset", width=60,
                           callback=lambda: _reset(state))

        dpg.add_text("Rotate the stick in a full circle.", color=C_DIM)

    return tags


def _start(state) -> None:
    state.circ_active = True


def _stop(state) -> None:
    state.circ_active = False


def _reset(state) -> None:
    state.circ_active = False
    state.circ_points.clear()


def update(tags: dict, state) -> None:
    _draw(tags["dl"], state.circ_points, state.circ_active)
