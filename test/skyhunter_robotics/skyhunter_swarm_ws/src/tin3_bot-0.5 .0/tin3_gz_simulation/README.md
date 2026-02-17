# tin3_gz_simulation

Gazebo simulation for TIN3 robot swarm.

---

## Dependencies

- `tin3_description`
- `tin3_gz_worlds`

---

## Usage

```bash
# Single robot
ros2 launch tin3_gz_simulation sim.launch.py

# Multi-robot
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=4

# With pattern
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=6 pattern:=circle

# With custom world
ros2 launch tin3_gz_simulation sim.launch.py world:=my_world.sdf

# Performance mode (no LiDAR)
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=8 lidar_mode:=none
```

---

## Launch Arguments

| Argument | Default | Options | Description |
|----------|---------|---------|-------------|
| `world` | `empty_world.sdf` | Any `.sdf` filename | World file (resolved from package) |
| `num_robots` | `1` | 1–8+ | Number of robots to spawn |
| `pose` | `0 0 0.5` | `"x y z"` or `"x y z R P Y"` | Spawn position (single robot) |
| `pattern` | `grid` | `grid`, `line_x`, `line_y`, `random`, `circle` | Multi-robot spawn pattern |
| `spacing` | `3.0` | meters | Distance between robots |
| `lidar_mode` | `full` | `full`, `half`, `low`, `none` | LiDAR resolution/disable |

---

## Multi-Robot Namespacing

With `num_robots` > 1, each robot gets namespace `/robot_XX/` (e.g., `/robot_01/`, `/robot_02/`). All topics, TF frames, and nodes are isolated per namespace.

**11 topics per robot:**

```
/robot_01/cmd_vel
/robot_01/gimbal/pan_cmd
/robot_01/gimbal/tilt_cmd
/robot_01/gps/fix
/robot_01/imu/data
/robot_01/ir_camera/image_raw
/robot_01/joint_states
/robot_01/odom
/robot_01/rgb_camera/image_raw
/robot_01/robot_description
/robot_01/scan/points
```

---

## Sensor Topics (Single Robot)

| Sensor | Topic | Type | Rate (sim) |
|--------|-------|------|------------|
| LiDAR | `/scan/points` | `sensor_msgs/msg/PointCloud2` | — |
| IMU | `/imu/data` | `sensor_msgs/msg/Imu` | 100 Hz |
| GPS | `/gps/fix` | `gps_msgs/msg/GPSFix` | 5 Hz |
| RGB Camera | `/rgb_camera/image_raw` | `sensor_msgs/msg/Image` | 30 Hz |
| IR Camera | `/ir_camera/image_raw` | `sensor_msgs/msg/Image` | 30 Hz |
| Gimbal Pan | `/gimbal/pan_cmd` | `std_msgs/msg/Float64` | command |
| Gimbal Tilt | `/gimbal/tilt_cmd` | `std_msgs/msg/Float64` | command |

---

## Note

Adjust `z` in `pose` based on terrain height. Some worlds have elevated or rotated terrain — if robot falls through, increase z value (e.g., `pose:="0 0 2.0"`).