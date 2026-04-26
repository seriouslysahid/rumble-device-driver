#!/usr/bin/env python3
"""
rumble_teleop_node.py — ROS 2 teleoperation node for TurtleBot3
using the rumble Xbox controller character device driver.

Team: PathFinders

Overview
--------
This node reads raw controller input from /dev/rumble0 (the rumble kernel
driver) in a background thread and publishes geometry_msgs/Twist messages
to /cmd_vel for driving a TurtleBot3 robot.

Axis mapping
------------
  Left stick X  → Twist.angular.z   (±1.5 rad/s)
  Left stick Y  → Twist.linear.x    (±1.0 m/s)
  Deadzone: ±1500 raw units (~4.6% of full range)

Collision feedback
------------------
Subscribes to /odom or /bump (TurtleBot3 LDS / OpenCR bump topic).
On a detected collision the node fires the rumble motors at 80% for
300 ms to give the driver haptic feedback.

Publishing rate is capped at 20 Hz.
"""

import ctypes
import fcntl
import os
import struct
import threading
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry

# ---------------------------------------------------------------------------
# Shared kernel header constants (must match rumble.h)
# ---------------------------------------------------------------------------

# struct rumble_input layout (matches kernel struct with explicit padding):
#   uint16_t buttons
#   uint8_t  lt
#   uint8_t  rt
#   int16_t  lx, ly
#   int16_t  rx, ry
#   uint16_t _pad       (explicit padding for 8-byte alignment)
#   uint64_t timestamp_us
#
# Total: 2 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 8 = 22 bytes
RUMBLE_INPUT_FMT = '<HBBhhhhHQ'   # little-endian, H for padding field
RUMBLE_INPUT_SIZE = struct.calcsize(RUMBLE_INPUT_FMT)

# struct rumble_motors:
#   uint8_t left
#   uint8_t right
RUMBLE_MOTORS_FMT = 'BB'

# ioctl number for RUMBLE_SET_MOTORS
# _IOW('R', 1, struct rumble_motors)  →  _IOW(0x52, 1, 2-byte struct)
# _IOW = (3 << 30) | (size << 16) | (type << 8) | nr
#       = 0xC0000000 | (2 << 16) | (0x52 << 8) | 1
RUMBLE_IOC_MAGIC = ord('R')    # 0x52
_IOC_WRITE = 1
_IOC_SIZEBITS = 14
_IOC_TYPEBITS = 8
_IOC_NRBITS = 8
_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
_IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
_IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS


def _IOC(direction, ioc_type, nr, size):
    return (direction  << _IOC_DIRSHIFT)  | \
           (ioc_type   << _IOC_TYPESHIFT) | \
           (nr         << _IOC_NRSHIFT)   | \
           (size       << _IOC_SIZESHIFT)


def _IOW(ioc_type, nr, size):
    return _IOC(_IOC_WRITE, ioc_type, nr, size)


RUMBLE_SET_MOTORS = _IOW(RUMBLE_IOC_MAGIC, 1,
                          struct.calcsize(RUMBLE_MOTORS_FMT))

# ---------------------------------------------------------------------------
# Controller parameters
# ---------------------------------------------------------------------------
DEADZONE      = 1500          # raw units (~4.6 % of 32768)
MAX_LINEAR    = 1.0           # m/s
MAX_ANGULAR   = 1.5           # rad/s
AXIS_MAX      = 32768.0
PUBLISH_HZ    = 20.0          # maximum cmd_vel publish rate
DEV_PATH      = '/dev/rumble0'


def apply_deadzone(value: int, deadzone: int) -> int:
    """Return 0 if |value| < deadzone, else return value unchanged."""
    if abs(value) < deadzone:
        return 0
    return value


def axis_to_unit(raw: int, deadzone: int) -> float:
    """Map a signed 16-bit axis value to [-1.0, +1.0] with deadzone."""
    v = apply_deadzone(raw, deadzone)
    if v == 0:
        return 0.0
    return max(-1.0, min(1.0, v / AXIS_MAX))


# ---------------------------------------------------------------------------
# RumbleTeleopNode
# ---------------------------------------------------------------------------

class RumbleTeleopNode(Node):
    """ROS 2 node that reads /dev/rumble0 and drives a TurtleBot3."""

    def __init__(self):
        super().__init__('rumble_teleop')

        # Publisher: cmd_vel
        self._cmd_pub = self.create_publisher(Twist, '/cmd_vel', 10)

        # Subscriber: /odom for rudimentary collision detection
        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        self._odom_sub = self.create_subscription(
            Odometry, '/odom', self._odom_cb, qos)

        # Internal state
        self._fd = -1
        self._connected = False
        self._last_twist = Twist()
        self._last_pub_time = 0.0
        self._pub_interval = 1.0 / PUBLISH_HZ
        self._lock = threading.Lock()

        # Collision detection — track linear velocity from odom
        self._prev_odom_linear_x = 0.0
        self._collision_cooldown = 0.0   # wall-clock time until next allowed rumble

        # Start background reader thread
        self._stop_event = threading.Event()
        self._reader_thread = threading.Thread(
            target=self._reader_loop, daemon=True, name='rumble_reader')
        self._reader_thread.start()

        self.get_logger().info('rumble_teleop node started')

    # -----------------------------------------------------------------------
    # Background reader thread
    # -----------------------------------------------------------------------

    def _open_device(self) -> bool:
        """Attempt to open /dev/rumble0.  Returns True on success."""
        try:
            fd = os.open(DEV_PATH, os.O_RDWR | os.O_NONBLOCK)
            self._fd = fd
            self._connected = True
            self.get_logger().info(f'Controller connected: {DEV_PATH}')
            return True
        except OSError as exc:
            self.get_logger().warn(
                f'Cannot open {DEV_PATH}: {exc} — retrying in 2 s')
            return False

    def _close_device(self):
        if self._fd >= 0:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = -1
        if self._connected:
            self._connected = False
            self.get_logger().warn('Controller disconnected')
            # Stop the robot
            self._publish_twist(Twist())

    def _reader_loop(self):
        """Continuously read packets from /dev/rumble0 and publish Twist."""
        while not self._stop_event.is_set():
            # (Re-)connect if needed
            if self._fd < 0:
                if not self._open_device():
                    time.sleep(2.0)
                    continue

            # Poll the fd with a short timeout so we can check stop_event
            import select
            try:
                readable, _, _ = select.select([self._fd], [], [], 0.05)
            except (OSError, ValueError):
                self._close_device()
                continue

            if not readable:
                continue

            # Read one packet
            try:
                raw = os.read(self._fd, RUMBLE_INPUT_SIZE)
            except BlockingIOError:
                continue
            except OSError as exc:
                self.get_logger().warn(f'read error: {exc}')
                self._close_device()
                continue

            if len(raw) != RUMBLE_INPUT_SIZE:
                self.get_logger().warn(
                    f'Short read: {len(raw)} / {RUMBLE_INPUT_SIZE} bytes')
                continue

            # Unpack the struct rumble_input
            (buttons, lt, rt, lx, ly, rx, ry, _pad,
             timestamp_us) = struct.unpack(RUMBLE_INPUT_FMT, raw)

            # Build Twist from left stick
            twist = Twist()
            twist.linear.x  =  axis_to_unit(ly, DEADZONE) * MAX_LINEAR
            twist.angular.z = -axis_to_unit(lx, DEADZONE) * MAX_ANGULAR

            # Rate-limit publishing
            now = time.monotonic()
            if now - self._last_pub_time >= self._pub_interval:
                self._last_pub_time = now
                self._cmd_pub.publish(twist)

            with self._lock:
                self._last_twist = twist

    # -----------------------------------------------------------------------
    # /odom subscriber — simple collision heuristic
    # -----------------------------------------------------------------------

    def _odom_cb(self, msg: Odometry):
        """
        Detect sudden deceleration as a proxy for collision.

        If the robot was moving forward (linear.x > 0.1 m/s) and the
        reported velocity drops to near-zero in one odom update, treat
        it as a collision and fire rumble feedback.
        """
        current_vel = msg.twist.twist.linear.x
        prev_vel = self._prev_odom_linear_x
        self._prev_odom_linear_x = current_vel

        # Collision heuristic: rapid deceleration while moving
        delta = prev_vel - current_vel
        if prev_vel > 0.15 and delta > 0.15 and abs(current_vel) < 0.05:
            now = time.monotonic()
            if now > self._collision_cooldown:
                self._collision_cooldown = now + 2.0   # 2 s cooldown
                self.get_logger().warn('Collision detected — firing rumble feedback')
                threading.Thread(
                    target=self._rumble_feedback,
                    args=(80, 80, 300),
                    daemon=True).start()

    # -----------------------------------------------------------------------
    # Rumble helpers
    # -----------------------------------------------------------------------

    def _rumble_feedback(self, left_pct: int, right_pct: int, ms: int):
        """Send a timed rumble pulse.  Runs in its own thread."""
        if self._fd < 0:
            return
        try:
            self._set_motors(left_pct, right_pct)
            time.sleep(ms / 1000.0)
            self._set_motors(0, 0)
        except OSError as exc:
            self.get_logger().warn(f'Rumble ioctl failed: {exc}')

    def _set_motors(self, left_pct: int, right_pct: int):
        """Issue RUMBLE_SET_MOTORS ioctl on the open device fd."""
        if self._fd < 0:
            return
        buf = struct.pack(RUMBLE_MOTORS_FMT, left_pct, right_pct)
        fcntl.ioctl(self._fd, RUMBLE_SET_MOTORS, bytearray(buf))

    def _publish_twist(self, twist: Twist):
        self._cmd_pub.publish(twist)

    # -----------------------------------------------------------------------
    # Lifecycle
    # -----------------------------------------------------------------------

    def destroy_node(self):
        self.get_logger().info('Shutting down rumble_teleop ...')
        self._stop_event.set()
        self._reader_thread.join(timeout=2.0)
        self._close_device()
        # Send a zero-velocity command before exiting
        self._cmd_pub.publish(Twist())
        super().destroy_node()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(args=None):
    rclpy.init(args=args)
    node = RumbleTeleopNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
