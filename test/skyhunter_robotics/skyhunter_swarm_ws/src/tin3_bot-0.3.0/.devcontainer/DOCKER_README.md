# Docker Setup for tin3_bot (ROS 2 Humble Testing)

Test tin3_bot in ROS 2 Humble environment. Works on any machine 
## Folder Structure

```
tin3_bot/                    
├── .devcontainer/
│   ├── Dockerfile           
│   ├── devcontainer.json 
├── urdf/
├── launch/
├── config/
├── meshes/
├── worlds/
└── package.xml
```

## Quick Start

### Method 1: VS Code Dev Container (Recommended)

1. **Open in VS Code:**
   ```bash
   code tin3_bot/
   ```
23. **Reopen in Container:**
   - Press `Ctrl+Shift+P`
   - Select "Dev Containers: Reopen in Container"
   - Wait for build (~5-10 min first time)

3. **Test:**
   ```bash
   # Already in /ros2_ws, already built
   source install/setup.bash
   ros2 launch tin3_bot sim.launch.py
   ```

## Environment

| Component | Version |
|-----------|---------|
| ROS 2 | Humble |
| Gazebo | Harmonic |
| Ubuntu | 22.04 |

## Installed Packages

- ros-humble-ros-gz (bridge)
- ros-humble-robot-localization
- ros-humble-navigation2
- ros-humble-slam-toolbox
- ros-humble-xacro
- ros-humble-gps-msgs
- ros-humble-rviz2
- ros-humble-plotjuggler-ros

## Testing Commands

```bash
# Single robot
ros2 launch tin3_bot sim.launch.py

# Multi-robot with low LiDAR
ros2 launch tin3_bot sim.launch.py num_robots:=4 lidar_mode:=low

# With EKF
ros2 launch tin3_bot sim.launch.py use_ekf:=true
```
