"""timing.py — Packet rate and inter-packet timing plot."""

import dearpygui.dearpygui as dpg
from ...ui.theme import C_DIM, C_ACCENT, C_TEXT

_PLOT_W = 260
_PLOT_H = 80


def build() -> dict:
    tags: dict = {}
    dpg.add_text("Timing", color=C_DIM)
    with dpg.group(horizontal=True):
        dpg.add_text("Rate:", color=C_DIM)
        dpg.add_text("—", tag="tm_rate"); tags["rate"] = "tm_rate"
        dpg.add_spacer(width=10)
        dpg.add_text("Mean:", color=C_DIM)
        dpg.add_text("—", tag="tm_mean"); tags["mean"] = "tm_mean"
        dpg.add_spacer(width=10)
        dpg.add_text("Jitter:", color=C_DIM)
        dpg.add_text("—", tag="tm_jitter"); tags["jitter"] = "tm_jitter"

    with dpg.plot(label="", width=_PLOT_W, height=_PLOT_H,
                  no_title=True, no_mouse_pos=True,
                  no_menus=True) as plot:
        tags["plot"] = plot
        dpg.add_plot_axis(dpg.mvXAxis, label="", no_tick_labels=True,
                          tag="tm_xax")
        dpg.add_plot_axis(dpg.mvYAxis, label="ms", tag="tm_yax")
        tags["series"] = dpg.add_line_series([], [], label="Δt ms",
                                             parent="tm_yax")

    return tags


def update(tags: dict, state) -> None:
    dpg.set_value(tags["rate"],
        f"{state.rate_hz:.1f} Hz" if state.rate_hz > 0 else "—")
    dpg.set_value(tags["mean"],
        f"{state.mean_ms:.2f} ms" if state.mean_ms > 0 else "—")
    dpg.set_value(tags["jitter"],
        f"{state.jitter_ms:.2f} ms" if state.jitter_ms > 0 else "—")

    vals = list(state.intervals_ms)
    if vals:
        xs = list(range(len(vals)))
        dpg.set_value(tags["series"], [xs, vals])
        dpg.set_axis_limits("tm_xax", 0, max(len(vals) - 1, 1))
        lo = max(0, min(vals) - 1)
        hi = max(vals) + 1
        dpg.set_axis_limits("tm_yax", lo, hi)
    else:
        dpg.set_value(tags["series"], [[], []])
