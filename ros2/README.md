# rumble — ROS 2 TurtleBot3 Teleoperation with Xbox 1708

> **Team:** PathFinders

This directory contains the ROS 2 Python package `rumble_teleop`.  It reads
raw Xbox controller data from `/dev/rumble0` (provided by the `rumble` kernel
driver) and drives a TurtleBot3 robot via `/cmd_vel`.  Collision events
detected through `/odom` trigger haptic rumble feedback to the operator.

---

## Package layout

```
ros2/
├── rumble_teleop/
│   ├── rumble_teleop_node.py   # Main ROS 2 node
│   ├── package.xml
│   └── setup.py
└── README.md                   # This file
```

---

## Prerequisites

| Dependency | Version | Install |
|------------|---------|---------|
| ROS 2      | Humble or Iron | [docs.ros.org](https://docs.ros.org) |
| TurtleBot3 packages | matching ROS 2 distro | `sudo apt install ros-humble-turtlebot3*` |
| Gazebo Classic | 11.x | `sudo apt install ros-humble-gazebo-ros-pkgs` |
| rumble kernel module | any | see top-level README |
| Python 3.10+ | — | system default on Ubuntu 22.04 |

Set the TurtleBot3 model before launching:

```bash
export TURTLEBOT3_MODEL=burger   # or waffle / waffle_pi
```

---

## Building the ROS 2 package

```bash
# From your ROS 2 workspace root (e.g. ~/ros2_ws)
mkdir -p ~/ros2_ws/src
cp -r /path/to/rumble/ros2/rumble_teleop ~/ros2_ws/src/

source /opt/ros/humble/setup.bash
cd ~/ros2_ws
colcon build --packages-select rumble_teleop
source install/setup.bash
```

---

## Running the teleop node

Make sure the `rumble` kernel module is loaded and a controller is connected
before starting the node:

```bash
# Load the driver (once per boot, or after plug-in)
sudo insmod /path/to/rumble/driver/rumble.ko
# Grant read/write access (or use the udev rule from the top-level README)
sudo chmod a+rw /dev/rumble0

# In a separate terminal — source ROS 2 and start the node
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
ros2 run rumble_teleop rumble_teleop_node
```

### Expected log output

```
[INFO] [rumble_teleop]: rumble_teleop node started
[INFO] [rumble_teleop]: Controller connected: /dev/rumble0
```

---

## TurtleBot3 Gazebo demo

### 1. Launch the TurtleBot3 simulation

```bash
# Terminal 1 — Gazebo world
source /opt/ros/humble/setup.bash
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py
```

### 2. (Optional) Launch the navigation stack

```bash
# Terminal 2 — Nav2 (skip if you only want manual teleoperation)
source /opt/ros/humble/setup.bash
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_navigation2 navigation2.launch.py use_sim_time:=True
```

### 3. Start the rumble teleop node

```bash
# Terminal 3
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
ros2 run rumble_teleop rumble_teleop_node
```

### 4. Drive the robot

- **Left stick Y** — forward / backward (±1.0 m/s)
- **Left stick X** — rotate left / right (±1.5 rad/s)
- Deadzone: ±1500 raw units (~4.6 % of full range)
- Commands are published to `/cmd_vel` at up to 20 Hz

### 5. Collision feedback

When the node detects a rapid velocity drop in `/odom` data (indicative of a
wall collision), it fires the controller's rumble motors at 80 % intensity for
300 ms, giving the operator immediate haptic notification.

---

## Verifying the topic stream

```bash
# Watch /cmd_vel in a separate terminal
ros2 topic echo /cmd_vel

# Check publish rate
ros2 topic hz /cmd_vel
```

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---------|-------------|-----|
| Node prints "Cannot open /dev/rumble0" | Module not loaded or permission denied | `sudo insmod rumble.ko` and check udev rule |
| `/cmd_vel` not received by Gazebo | Namespace mismatch | Confirm `TURTLEBOT3_MODEL` is set and Gazebo bridge is running |
| No odom-based collision detection | `/odom` not published | Ensure robot_state_publisher and Gazebo plugin are active |
| Rumble ioctl fails | Controller disconnected mid-run | Reconnect controller; node retries automatically |

---

## Notes

- The node retries opening `/dev/rumble0` every 2 seconds after a disconnect,
  so it recovers automatically when the controller is re-plugged.
- Only the **left stick** axes are mapped to motion commands by default.
  The right stick and buttons are parsed by the driver and available in the
  raw `struct rumble_input` stream but not currently used by this node.
- To change the linear / angular speed limits, edit the `MAX_LINEAR` and
  `MAX_ANGULAR` constants at the top of `rumble_teleop_node.py`.
