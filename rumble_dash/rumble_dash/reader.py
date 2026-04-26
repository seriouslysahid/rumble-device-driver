"""
reader.py — Daemon reader thread for /dev/rumble0.

Architecture:
  reader thread → bounded queue → UI thread

Events pushed to the queue:
  ("connected",  device_path: str)
  ("sample",     Sample)
  ("error",      message: str)
  ("disconnected", None)
"""

import os
import select
import threading
import time
from queue import Queue, Full

from .abi import INPUT_SIZE, parse

DEVICE = "/dev/rumble0"
RECONNECT_DELAY = 1.0   # seconds between reconnect attempts
QUEUE_MAXSIZE   = 256   # drop oldest if full


def _drop_push(q: Queue, item: object) -> None:
    """Push item; if queue is full, drop the oldest entry first."""
    try:
        q.put_nowait(item)
    except Full:
        try:
            q.get_nowait()
        except Exception:
            pass
        try:
            q.put_nowait(item)
        except Full:
            pass


def _reader_loop(q: Queue, stop: threading.Event, device: str) -> None:
    """Inner loop: open device, poll, read, parse, push events."""
    while not stop.is_set():
        # ── open ──────────────────────────────────────────────────────────
        try:
            fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as exc:
            _drop_push(q, ("error", f"open: {exc}"))
            stop.wait(RECONNECT_DELAY)
            continue

        _drop_push(q, ("connected", device))

        poller = select.poll()
        poller.register(fd, select.POLLIN | select.POLLERR | select.POLLHUP)

        # ── read loop ─────────────────────────────────────────────────────
        try:
            while not stop.is_set():
                events = poller.poll(200)   # 200 ms timeout → check stop flag
                if not events:
                    continue

                flags = events[0][1]

                if flags & (select.POLLERR | select.POLLHUP):
                    _drop_push(q, ("disconnected", None))
                    break

                if flags & select.POLLIN:
                    recv_ns = time.monotonic_ns()
                    try:
                        raw = os.read(fd, INPUT_SIZE)
                    except OSError as exc:
                        import errno as _errno
                        if exc.errno in (_errno.ENODEV, _errno.ENOENT):
                            _drop_push(q, ("disconnected", None))
                        else:
                            _drop_push(q, ("error", f"read: {exc}"))
                        break

                    if len(raw) == 0:
                        # EOF → disconnect
                        _drop_push(q, ("disconnected", None))
                        break

                    if len(raw) != INPUT_SIZE:
                        _drop_push(q, ("error",
                            f"short read: {len(raw)}/{INPUT_SIZE} bytes"))
                        continue

                    try:
                        sample = parse(raw, recv_ns)
                        _drop_push(q, ("sample", sample))
                    except ValueError as exc:
                        _drop_push(q, ("error", f"parse: {exc}"))

        finally:
            poller.unregister(fd)
            os.close(fd)

        if not stop.is_set():
            stop.wait(RECONNECT_DELAY)


def start(device: str = DEVICE) -> tuple[Queue, threading.Event]:
    """
    Start the reader daemon thread.

    Returns:
        q     – bounded Queue of (event_type, payload) tuples
        stop  – set this Event to request shutdown
    """
    q    = Queue(maxsize=QUEUE_MAXSIZE)
    stop = threading.Event()
    t    = threading.Thread(
        target=_reader_loop,
        args=(q, stop, device),
        name="rumble-reader",
        daemon=True,
    )
    t.start()
    return q, stop
