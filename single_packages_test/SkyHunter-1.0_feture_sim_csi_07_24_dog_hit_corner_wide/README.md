# Combat Robot Operation System Project

**ROS2-based Combat Robot System with Hailo AI Acceleration**

<p align='center'>
    <img src="./docs/images/skyautonet_logo.png" alt="drawing" width="400"/>
</p>

## 1. Project Overview

This project implements a comprehensive software system for a combat robot featuring remote control capabilities and autonomous functions. It leverages the **Hailo AI accelerator** for real-time object detection and tracking, precise **Pan/Tilt control** for targeting, and integrates various sensors to maximize mission performance.

### Key Objectives
*   **Real-time AI Detection**: Detect and track specific targets (Human, Drone) using Hailo8.
*   **Precise Visual Servoing**: Auto-aiming control via Pan/Tilt mechanism.
*   **Remote Operation**: Tablet/App-based teleoperation and monitoring.
*   **Robust System Control**: Finite State Machine (FSM) based operation management.

## 2. Key Features

- **Remote Control**:
    - Tablet-based control via TCP/UDP.
    - Joystick/Virtual stick input for chassis driving.
    - Touch-to-aim interface.
- **AI Object Detection & Tracking**:
    - **Hardware**: Hailo8 AI Module.
    - **Models**: YoloV5s/Yolo11s optimized for Person and Drone detection.
    - **Tracking**: SORT algorithm with Kalman Filter.
- **Pan/Tilt Control**:
    - Custom serial protocol driver.
    - PID-based visual servoing for target centering.
    - Automatic scanning modes (Surveillance).
- **Weapon System**:
    - PWM-based Gun Trigger control.
    - Safety interlocks and fire permission handling.
- **Visualization**:
    - RTSP Stream Server for remote video feed.
    - ImGui-based debug interface overlay.

## 3. Project Structure

```
combatrobot_1/
├── docs/                             # Documentation (Manuals, Standards)
├── ros/                              # ROS2 Workspace
│   └── src/
│       └── skyautonet/
│           ├── combat_robot_launch/          # Main System Launch Files
│           ├── combat_robot_system/          # Core Functionality Packages
│           │   ├── camera_interface/         # Camera Driver
│           │   ├── combat_robot_msgs/        # Custom ROS Messages
│           │   ├── combat_robot_operation_system/ # Main FSM Node
│           │   ├── gun_trigger/              # Weapon Trigger Control
│           │   ├── human_detector/           # Hailo AI Detection Node
│           │   ├── imu_publisher/            # IMU Sensor Interface
│           │   ├── laser_distance/           # Laser Rangefinder Interface
│           │   ├── pan_tilt_controller/      # Pan/Tilt Serial Driver
│           │   ├── robot_server/             # Network Interface (App Server)
│           │   └── teleop_controller/        # Teleoperation Logic
│           └── combat_robot_visualization/   # Visualization Tools
├── scripts/                          # Setup & Utility Scripts
└── test/                             # Testing Scripts
```

## 4. Hardware & Software Requirements

### Hardware
- **SBC**: Rockchip RK3588 (e.g., Orange Pi 5, Rock 5B)
- **AI Accelerator**: Hailo8 M.2 Module
- **Camera**: USB or MIPI Camera
- **Actuators**: Pan/Tilt Unit (Serial Control), Gun Trigger Servo (PWM)
- **Sensors**: IMU, Laser Distance Sensor

### Software
- **OS**: Ubuntu 20.04 (Focal) or 22.04 (Jammy)
- **ROS Version**: ROS2 Galactic or Humble
- **Dependencies**:
    - HailoRT (Hailo Runtime)
    - OpenCV 4.x
    - Boost

## 5. Installation & Build

### 5.1. Setup Dependencies
Run the provided scripts to install drivers and configure the system.

```bash
# Install Hailo Drivers
./scripts/install_hailo_driver.sh

# Configure gun-trigger PWM (if hardware present)
./scripts/gun_trigger.sh
```

### 5.2. Build ROS2 Packages

```bash
# Go to ROS workspace
cd ros

# Install ROS dependencies
rosdep install --from-paths src --ignore-src -r -y

# Build packages
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
```

## 6. Usage

### 6.1. Running the System

Three one-shot launcher scripts wrap the env setup → workspace overlay → permission setup → `ros2 launch` chain. Each accepts `--build` to colcon-build first and `--` to pass extra args verbatim to `ros2 launch`.

```bash
# Bench test with dummy data (no hardware required)
./scripts/run_test.sh

# Demo deployment (forward → scan → fire-per-target → reverse sequence)
./scripts/run_demo.sh

# Production deployment (full FSM, real hardware)
./scripts/run_production.sh
```

Examples:

```bash
./scripts/run_test.sh --build -- scenario:=attack_fire
./scripts/run_production.sh -- use_swarm_coordinator:=true swarm_role:=follower robot_id:=2 leader_robot_id:=1
```

If you prefer to run launch directly:

```bash
source ros/install/setup.bash
ros2 launch combat_robot_launch combat_robot.launch.xml
```

**Launch Arguments:**
- `deployment_mode`: `production` | `demo` | `office_test` — auto-selects the matching `params.<mode>.yaml` overlay (default: `production`)
- `use_detector`: Enable AI detection (default: true)
- `use_operator`: Enable Main OS & Pan/Tilt (default: true)
- `use_robot_server`: Enable App Connection Server (default: true)
- `use_visualization`: Enable ImGui Display (default: false)

### 6.2. Network Interface (App Connection)
The `robot_server` node exposes the following ports for remote applications:
- **Command (TCP 65432)**: Mode switching, PTZ control.
- **Touch (UDP 65433)**: Touch-to-aim coordinates.
- **Driving (UDP 65434)**: Chassis movement commands.
- **Status (TCP 65435)**: System status feedback.

## 7. Key ROS Packages

- **`combat_robot_operation_system`**: The central brain. Manages states (IDLE, SURVEILLANCE, TRACKING, ATTACKING) and coordinates between detection, user commands, and hardware actuation.
- **`human_detector`**: Wraps the Hailo8 inference engine. Publishes `DetectedObjects` and calculates the optimal `TargetPoint` for the tracker.
- **`pan_tilt_controller`**: Interfaces with the Pan/Tilt hardware via serial. Implements PID control for smooth tracking.
- **`robot_server`**: Bridges ROS2 topics with external TCP/UDP clients (e.g., Android Tablet).

## 8. Coding Standards & Contribution

Please adhere to the project's coding standards found in `docs/coding_standards/`.
- [Internal Style Guide](./docs/coding_standards/codingstandards-style.md)
- [Performance & Safety](./docs/coding_standards/codingstandards-performance-safety.md)

## 9. Contact

- **Skyautonet Senior Researcher** (<kijong.gong@skyautonet.com>)
- **Skyautonet Senior Researcher** (<sanghyuk.bae@skyautonet.com>)
- **Skyautonet Senior Developer** (<kyunghwan.kim@skyautonet.com>)
- **Skyautonet Developer** (<damgi.ahn@skyautonet.com>)
