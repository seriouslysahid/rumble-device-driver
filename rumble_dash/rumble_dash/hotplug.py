"""
hotplug.py — Optional pyudev-based USB hotplug monitor.

If pyudev is not installed, start() is a no-op and logs one message.
The reader thread remains authoritative; hotplug only improves latency.

Watches for Xbox controller USB events (VID 045E) to nudge reconnection.
"""

import threading
from queue import Queue

_VENDOR  = "045e"
_PIDS    = {"02fd", "02dd", "02ea"}   # known wired Xbox 1708 PIDs


def _monitor_loop(q: Queue, stop: threading.Event) -> None:
    import pyudev  # type: ignore
    context = pyudev.Context()
    monitor = pyudev.Monitor.from_netlink(context)
    monitor.filter_by(subsystem="usb")
    monitor.start()

    monitor_fd = monitor.fileno()
    import select as _sel

    while not stop.is_set():
        r, _, _ = _sel.select([monitor_fd], [], [], 0.5)
        if not r:
            continue
        device = monitor.poll(0)
        if device is None:
            continue
        vid = (device.get("ID_VENDOR_ID") or "").lower()
        pid = (device.get("ID_MODEL_ID")  or "").lower()
        if vid != _VENDOR or pid not in _PIDS:
            continue
        action = device.action
        if action == "add":
            q.put(("hotplug_add", None))
        elif action == "remove":
            q.put(("hotplug_remove", None))


def start(q: Queue, stop: threading.Event) -> None:
    """
    Attempt to start the pyudev monitor thread.
    Silently skips if pyudev is unavailable.
    """
    try:
        import pyudev  # noqa: F401
    except ImportError:
        print("[rumble-dash] pyudev not installed — hotplug monitoring disabled")
        return

    t = threading.Thread(
        target=_monitor_loop,
        args=(q, stop),
        name="rumble-hotplug",
        daemon=True,
    )
    t.start()
