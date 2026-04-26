"""triggers.py — LT / RT trigger bars."""

import dearpygui.dearpygui as dpg
from ...ui.theme import C_DIM, C_TRIG_BG, C_TRIG_FILL, C_TEXT

_BAR_W = 30
_BAR_H = 120


def _draw_trigger(dl: str, value: float, label: str) -> None:
    """value in [0, 1]."""
    dpg.delete_item(dl, children_only=True)
    # Background
    dpg.draw_rectangle([0, 0], [_BAR_W, _BAR_H],
                        color=C_TRIG_BG, fill=C_TRIG_BG, parent=dl)
    # Fill from bottom
    fill_h = int(_BAR_H * value)
    if fill_h > 0:
        dpg.draw_rectangle([0, _BAR_H - fill_h], [_BAR_W, _BAR_H],
                            color=C_TRIG_FILL, fill=C_TRIG_FILL, parent=dl)
    # Border
    dpg.draw_rectangle([0, 0], [_BAR_W, _BAR_H],
                        color=(60, 75, 95, 200), fill=(0,0,0,0),
                        thickness=1.5, parent=dl)
    # Label
    dpg.draw_text([4, 4], label, color=C_DIM, size=12, parent=dl)


def build() -> dict:
    tags: dict = {}
    with dpg.group(horizontal=True):
        with dpg.group():
            dpg.add_text("LT", color=C_DIM)
            tags["lt_dl"] = dpg.add_drawlist(width=_BAR_W, height=_BAR_H)
            dpg.add_text("0", tag="lt_val")
            tags["lt_val"] = "lt_val"

        dpg.add_spacer(width=12)

        with dpg.group():
            dpg.add_text("RT", color=C_DIM)
            tags["rt_dl"] = dpg.add_drawlist(width=_BAR_W, height=_BAR_H)
            dpg.add_text("0", tag="rt_val")
            tags["rt_val"] = "rt_val"

    return tags


def update(tags: dict, state) -> None:
    s = state.sample
    lt = s.lt_n if s else 0.0
    rt = s.rt_n if s else 0.0
    _draw_trigger(tags["lt_dl"], lt, "LT")
    _draw_trigger(tags["rt_dl"], rt, "RT")
    dpg.set_value(tags["lt_val"], f"{s.lt}" if s else "0")
    dpg.set_value(tags["rt_val"], f"{s.rt}" if s else "0")
