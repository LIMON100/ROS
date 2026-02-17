# TIN3 Bot — Multi-Robot Tactical UGV Simulation

Gazebo Harmonic simulation of a tracked Unmanned Ground Vehicle (UGV) with full sensor suite, designed for single-robot autonomy and multi-robot swarm scenarios.

Spawn up to 8 robots simultaneously in Gazebo, each with 3D LiDAR, IMU, GPS, RGB/IR cameras on a pan-tilt gimbal, differential drive, and Nav2/EKF compatibility.

---

## Requirements

- **ROS 2:** Jazzy (development), Humble (delivery)
- **Simulator:** Gazebo Harmonic (gz-sim)
- **GPU:** NVIDIA recommended (tested on Quadro RTX 4000)

### Install Dependencies

```bash
# Recommended: install all dependencies via rosdep
cd <your_workspace>
rosdep install --from-paths src --ignore-src -r -y

# Manual install (if rosdep misses any)
sudo apt install ros-${ROS_DISTRO}-robot-localization
sudo apt install ros-${ROS_DISTRO}-nav2-bringup ros-${ROS_DISTRO}-nav2-smac-planner
sudo apt install ros-${ROS_DISTRO}-pointcloud-to-laserscan
sudo apt install ros-${ROS_DISTRO}-ros-gz
```

---

## Quick Start

```bash
# Build
cd <your_workspace>
colcon build
source install/setup.bash

# Single robot
ros2 launch tin3_gz_simulation sim.launch.py

# Multi-robot (4 robots in grid)
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=4 pattern:=grid spacing:=3.0

# 8 robots, performance mode (no LiDAR)
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=8 lidar_mode:=none
```

---

## Packages

| Package | Description |
|---------|-------------|
| `tin3_description` | Robot URDF/Xacro, DAE meshes, RViz launch |
| `tin3_gz_simulation` | Gazebo launch, bridge config, spawn logic |
| `tin3_gz_worlds` | SDF world files |
| `tin3_navigation` | Nav2 config, EKF (robot_localization), RViz nav config |

---

## Robot Specifications

| Parameter | Value |
|-----------|-------|
| Type | Tracked UGV (differential drive approximation) |
| Dimensions (L × W) | 1314 × 850 mm |
| Max speed | 1.94 m/s (7 km/h) |
| Drive | 6-wheel differential (hidden wheels, static track meshes) |
| TF frames | 23 (root: odom) |
| Collision | Box/cylinder primitives (optimized for multi-robot) |

---

## Sensor Suite

| Sensor | Topic | Rate | Details |
|--------|-------|------|---------|
| 3D LiDAR | `scan/points` | PointCloud2 | 120° H × 90° V, 0.1–30m, Gaussian noise σ=0.05m |
| IMU | `imu/data` | 100 Hz | sensor_msgs/Imu |
| GPS | `gps/fix` | 5 Hz | gps_msgs/GPSFix |
| RGB Camera | `rgb_camera/image_raw` | 30 Hz | 640×480 R8G8B8 |
| IR Camera | `ir_camera/image_raw` | 30 Hz | 640×480 L8 (grayscale) |
| Gimbal Pan | `gimbal/pan_cmd` | Float64 | std_msgs/Float64 |
| Gimbal Tilt | `gimbal/tilt_cmd` | Float64 | std_msgs/Float64 |

Multi-robot topics are namespaced: `/robot_XX/<topic>` (e.g., `/robot_01/imu/data`). Sensor rates shown are simulation rates; wall clock rates scale with RTF.

---

## Launch Arguments

| Argument | Default | Options | Description |
|----------|---------|---------|-------------|
| `world` | `empty_world.sdf` | Any `.sdf` file | World file path |
| `num_robots` | `1` | 1–8+ | Number of robots to spawn |
| `pose` | `0 0 0.5` | `"x y z"` or `"x y z R P Y"` | Spawn position |
| `pattern` | `grid` | `grid`, `line_x`, `line_y`, `random`, `circle` | Multi-robot spawn pattern |
| `spacing` | `3.0` | meters | Distance between robots |
| `lidar_mode` | `full` | `full`, `half`, `low`, `none` | LiDAR resolution/disable |

---

## Multi-Robot Namespacing

Each robot gets a namespace `/robot_XX/` (e.g., `/robot_01/`, `/robot_02/`). All topics, TF frames, and nodes are isolated per namespace.

**Per-robot topics (11 each):**

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

**Global topics:** `/clock`, `/tf`, `/tf_static`, `/rosout`, `/parameter_events`

---

## Navigation & Localization

```bash
# EKF only (sensor fusion: odom + IMU → /odom_filtered at 30 Hz)
ros2 launch tin3_navigation ekf.launch.py

# Full Nav2 stack (includes EKF)
ros2 launch tin3_navigation nav2_launch.py
```

EKF and Nav2 are currently configured for single-robot operation.

---

## URDF Structure

```
odom (TF root)
└── base_footprint
    └── base_link (chassis)
        ├── left_track_visual          ├── right_track_visual
        ├── left_front_wheel           ├── right_front_wheel
        ├── left_mid_wheel             ├── right_mid_wheel
        ├── left_rear_wheel            ├── right_rear_wheel
        ├── lidar_link
        ├── imu_link
        ├── gps_link
        └── gimbal_base_link
            └── gimbal_pan_link
                └── gimbal_tilt_link
                    ├── camera_holder_link
                    │   ├── rgb_camera_link → rgb_camera_optical_frame
                    │   └── ir_camera_link → ir_camera_optical_frame
                    └── rifle_link
```

---

## Performance (RTF Benchmarks)

Tested on NVIDIA Quadro RTX 4000, Driver 570, X11.

| Robots | RTF | Notes |
|--------|-----|-------|
| 1 | 95% | Full sensors |
| 4 | 40% | Full sensors |
| 8 | 20% | Full sensors |
| 8 | 21% | `lidar_mode:=none` (cameras are the bottleneck) |

RTF scales linearly at ~10% cost per additional robot. Disabling LiDAR had negligible impact — bottleneck is not LiDAR. Further profiling needed (camera disable test, physics rate reduction) to identify primary limiter.

---

## Known Limitations

- RTF drops below real-time with 4+ robots at full sensor load — performance optimization is planned (see VD-08)

---

## File Structure

```
swarm_bot_src/
├── src/
│   ├── tin3_description/        # Robot model
│   │   ├── urdf/                # Xacro files (robot.urdf.xacro entry point)
│   │   ├── meshes/              # DAE meshes (chassis, tracks, gimbal, etc.)
│   │   ├── launch/              # view_robot.launch.py (RViz)
│   │   └── rviz/
│   ├── tin3_gz_simulation/      # Gazebo simulation
│   │   ├── launch/              # sim.launch.py (main entry)
│   │   ├── config/              # ros_gz_bridge.yaml
│   │   └── models/
│   ├── tin3_gz_worlds/          # World files
│   │   └── worlds/              # empty_world.sdf, etc.
│   └── tin3_navigation/         # Navigation
│       ├── launch/              # ekf_launch.py, nav2_launch.py
│       ├── config/              # ekf.yaml, nav2_params.yaml
│       └── rviz/                # navigation.rviz
└── README.md                    # This file
```