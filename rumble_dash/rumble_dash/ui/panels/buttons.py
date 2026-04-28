"""buttons.py — Xbox-style button grid."""

import dearpygui.dearpygui as dpg
from ...ui.theme import C_BTN_ON, C_BTN_OFF, C_DIM

# Button colours by group
_GROUP_ON = {
    "face":    (0,   200, 200, 255),
    "shoulder":(0,   200, 200, 255),
    "system":  (200, 200,   0, 255),
    "stick":   (180, 100, 220, 255),
    "dpad":    (0,   200, 200, 255),
}

# Layout rows: list of (mask, label) per row
_ROWS = [
    # Row 0: shoulders
    [(0x1000, "LB"), (0x2000, "RB")],
    # Row 1: face
    [(0x0040, "X"), (0x0080, "Y"), (0x0010, "A"), (0x0020, "B")],
    # Row 2: system + stick clicks
    [(0x0001, "MENU"), (0x0002, "VIEW"), (0x0004, "LS"), (0x0008, "RS")],
    # Row 3: dpad
    [(0x0100, "↑"), (0x0200, "↓"), (0x0400, "←"), (0x0800, "→")],
]

_BTN_W = 44
_BTN_H = 26


def build() -> dict:
    tags: dict = {}
    dpg.add_text("Buttons", color=C_DIM)
    for row in _ROWS:
        with dpg.group(horizontal=True):
            for mask, label in row:
                tag = f"btn_{mask:04x}"
                dpg.add_button(label=label, tag=tag,
                               width=_BTN_W, height=_BTN_H)
                tags[mask] = tag
        dpg.add_spacer(height=2)
    return tags


def update(tags: dict, state) -> None:
    s = state.sample
    buttons = s.buttons if s else 0
    for mask, tag in tags.items():
        pressed = bool(buttons & mask)
        _set_btn_color(tag, C_BTN_ON if pressed else C_BTN_OFF)


# Cache per-button themes to avoid recreating every frame
_btn_themes: dict[str, tuple] = {}   # tag -> (theme_id, last_color)


def _set_btn_color(tag: str, color: tuple) -> None:
    prev = _btn_themes.get(tag)
    if prev and prev[1] == color:
        return
    if prev:
        dpg.delete_item(prev[0])
    with dpg.theme() as t:
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button,        color)
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, color)
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive,  color)
    dpg.bind_item_theme(tag, t)
    _btn_themes[tag] = (t, color)
