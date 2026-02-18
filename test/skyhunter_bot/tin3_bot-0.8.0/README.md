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

# Single robot (empty world)
ros2 launch tin3_gz_simulation sim.launch.py

# Single robot (Route 66 world)
ros2 launch tin3_gz_simulation sim.launch.py world:=route_66.sdf pose:="2445.0 293.5 53.0 0 0 -3.023"

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
| `tin3_gz_worlds` | SDF world files, terrain models, meshes |
| `tin3_navigation` | Nav2 config, EKF (robot_localization), maps, RViz nav config |

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

## Worlds

| World | Description | Robot Spawn Pose |
|-------|-------------|-----------------|
| `empty_world.sdf` | Flat empty ground (default) | `0 0 0.5 0 0 0` |
| `route_66.sdf` | Route 66 terrain (5km × 5km, scale 1.0) | `2445.0 293.5 53.0 0 0 -3.023` |

### Route 66 World

The Route 66 world uses OBJ terrain meshes exported from Blender 2.79. The visual mesh includes terrain, background, and props. The collision mesh is a decimated terrain-only version for physics.

```bash
# Launch with Route 66
ros2 launch tin3_gz_simulation sim.launch.py world:=route_66.sdf pose:="2445.0 293.5 53.0 0 0 -3.023"
```

### Gazebo GUI Camera Pose

 Camera pose is set in the default Gazebo GUI config:

```
~/.gz/sim/8/gui.config
```

To set the camera for Route 66, edit the `<camera_pose>` line in `gui.config`:

```xml
<camera_pose>2450.32 294.37 58.07 0 0.025 -3.023</camera_pose>
```

This positions the camera view at the start of the Route 66 road, looking along the driving direction.

---

## Launch Arguments

| Argument | Default | Options | Description |
|----------|---------|---------|-------------|
| `world` | `empty_world.sdf` | Any `.sdf` file | World file name or path |
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
ros2 launch tin3_navigation ekf_launch.py

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
│   │   ├── hooks/               # Environment hooks for Gazebo resource paths
│   │   ├── models/              # Terrain models (route_66/, etc.)
│   │   └── worlds/              # empty_world.sdf, route_66.sdf, etc.
│   └── tin3_navigation/         # Navigation
│       ├── launch/              # ekf_launch.py, nav2_launch.py
│       ├── config/              # ekf.yaml, nav2_params.yaml
│       ├── maps/                # Map files (if any)
│       └── rviz/                # navigation.rviz
└── README.md                    # This file
```
## Performance Optimization (RTF)

Real-Time Factor (RTF) measures simulation speed relative to wall clock. RTF of 100% means the simulation runs in real time.

**Hardware tested:** NVIDIA Quadro RTX 4000 (8GB VRAM)

### Performance Profiles

| Profile | Robots | RTF (Empty) | RTF (Route 66) | Use Case |
|---------|--------|-------------|----------------|----------|
| Default | 1 | ~95% | ~95% | Single-robot, all sensors |
| Balanced | 8 | ~50% | ~40% | Multi-robot with vision |
| Max Performance | 8 | ~87% | ~90% | Multi-robot Nav2 testing |

### Default Profile

No changes needed. All sensors at full quality.

```bash
ros2 launch tin3_gz_simulation sim.launch.py
```

### Balanced Profile (Recommended for Multi-Robot)

Reduce camera FPS to 10 Hz and physics to 500 Hz.

**1. Camera FPS** — in `tin3_description/urdf/camera.xacro`:
```xml
<xacro:property name="camera_fps" value="10" />
```

**2. Physics rate** — in your world SDF file:
```xml
<max_step_size>0.002</max_step_size>
<real_time_factor>1.0</real_time_factor>
<real_time_update_rate>500</real_time_update_rate>
```

```bash
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=8
```

### Max Performance Profile

Disable cameras entirely and reduce LiDAR resolution.

**1. Disable cameras** — in `tin3_description/urdf/robot_urdf.xacro`, comment out:
```xml
<!-- <xacro:include filename="$(find tin3_description)/urdf/camera.xacro" /> -->
```

**2. Physics rate** — same as Balanced (500 Hz).

```bash
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=8 lidar_mode:=low
```

### LiDAR Modes

| Mode | Resolution (H×V) | Points/Scan | Launch Argument |
|------|-------------------|-------------|-----------------|
| Full | 192 × 144 | 27,648 | `lidar_mode:=full` (default) |
| Half | 96 × 72 | 6,912 | `lidar_mode:=half` |
| Low | 48 × 36 | 1,728 | `lidar_mode:=low` |
| None | Disabled | 0 | `lidar_mode:=none` |

### RTF Scaling by Robot Count

Single robot at default settings; multi-robot with Balanced profile:

| Robots | Empty World | Route 66 |
|--------|-------------|----------|
| 1 | ~95% | ~95% |
| 4 | ~65% | ~55% |
| 6 | ~56% | ~46% |
| 8 | ~50% | ~40% |

### Key Findings

- **Camera rendering** is the primary bottleneck. Each robot runs 2 cameras (RGB + IR), totaling 16 render streams at 8 robots.
- **Terrain meshes** (e.g., Route 66 at 96.3MB) only impact RTF when cameras render them. With cameras disabled, terrain has zero cost.
- **LiDAR** has minimal RTF impact (~1% when fully disabled).
- **Physics at 500 Hz** is stable for all scenarios — driving, turning, and slopes.
- **6 wheels required** per robot for proper skid-steer turning. Do not reduce to 4.

After editing xacro or SDF files, rebuild:
```bash
colcon build --packages-select tin3_description tin3_gz_simulation
source install/setup.bash
```