"""
app.py — DearPyGui application: window setup, panel wiring, render loop.

Layout (1280 × 760):
  ┌─────────────────────────────────────────────────────────────────┐
  │ Status strip                                                    │
  ├──────────────────────────────┬──────────────────────────────────┤
  │ Left column                  │ Right column                     │
  │  Sticks + Triggers           │  Buttons                         │
  │  Timing                      │  Rumble                          │
  │                              │  Debug (collapsible)             │
  ├──────────────────────────────┴──────────────────────────────────┤
  │ Circularity test (collapsible)                                  │
  └─────────────────────────────────────────────────────────────────┘
"""

import queue as _queue
import dearpygui.dearpygui as dpg

from ..store import State
from ..reader import start as reader_start
from ..hotplug import start as hotplug_start
from ..ui import theme
from ..ui.panels import (
    status as p_status,
    sticks as p_sticks,
    triggers as p_triggers,
    buttons as p_buttons,
    timing as p_timing,
    rumble as p_rumble,
    debug as p_debug,
    circularity as p_circ,
)

_WIN_W = 1280
_WIN_H = 780


def run(device: str = "/dev/rumble0") -> None:
    state = State(device_path=device)
    q, stop = reader_start(device)
    hotplug_start(q, stop)

    dpg.create_context()
    theme.apply()

    with dpg.window(tag="main", label="rumble-dash — Xbox 1708 Controller",
                    width=_WIN_W, height=_WIN_H,
                    no_resize=False, no_close=True,
                    no_collapse=True):

        # ── Status strip ──────────────────────────────────────────────────
        st_tags = p_status.build()
        dpg.add_separator()

        # ── Main body: two columns ────────────────────────────────────────
        with dpg.group(horizontal=True):

            # Left column
            with dpg.child_window(width=520, height=560, border=False):
                with dpg.group(horizontal=True):
                    sk_tags = p_sticks.build()
                    dpg.add_spacer(width=16)
                    tr_tags = p_triggers.build()

                dpg.add_spacer(height=12)
                dpg.add_separator()
                dpg.add_spacer(height=6)
                tm_tags = p_timing.build()

            dpg.add_spacer(width=10)

            # Right column
            with dpg.child_window(width=0, height=560, border=False):
                bt_tags = p_buttons.build()
                dpg.add_spacer(height=12)
                dpg.add_separator()
                dpg.add_spacer(height=6)
                rm_tags = p_rumble.build(state)
                dpg.add_spacer(height=12)
                dpg.add_separator()
                dpg.add_spacer(height=6)
                db_tags = p_debug.build()

        dpg.add_separator()
        ci_tags = p_circ.build(state)

    dpg.create_viewport(
        title="rumble-dash",
        width=_WIN_W, height=_WIN_H,
        min_width=900, min_height=600,
    )
    dpg.setup_dearpygui()
    dpg.set_primary_window("main", True)
    dpg.show_viewport()

    # ── Render loop ───────────────────────────────────────────────────────
    while dpg.is_dearpygui_running():
        # Drain the queue — all state mutation here, on the UI thread
        try:
            while True:
                ev_type, payload = q.get_nowait()
                state.apply(ev_type, payload)
        except _queue.Empty:
            pass

        # Update panels
        p_status.update(st_tags, state)
        p_sticks.update(sk_tags, state)
        p_triggers.update(tr_tags, state)
        p_buttons.update(bt_tags, state)
        p_timing.update(tm_tags, state)
        p_rumble.update(rm_tags, state)
        p_debug.update(db_tags, state)
        p_circ.update(ci_tags, state)

        dpg.render_dearpygui_frame()

    stop.set()
    dpg.destroy_context()
