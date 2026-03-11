## Current Development Status 


Swarm Navigation (Nav2)
🟢 COMPLETE
Leader global planning and follower local tracking are operational.

Boids / Formation Control
🟢 COMPLETE
Wedge, Column, Diamond, and Forward-V active. Inter-robot collision avoidance (1.5m safety bubble) is highly stable.

AI Perception (YOLOv8)
🟢 COMPLETE
Dedicated GPU is used and integrated via ONNX Runtime. "Person" detection running at sub-10ms latency.

Target Lock & Gimbal PID
🟢 COMPLETE
AI prioritizes closest physical threat. Gimbal autonomously tracks the calculated Lead Point.

Sensor Fusion (lidar-camera)
🟢 COMPLETE
OpenMP LiDAR processing active. Gimbal overrides AI to snap to blind-spot threats automatically.

Leader Succession (FSM)
🟠 NEEDS DEBUG
FSM logic and Raft voting are complete. Bug: If Leader is killed, SH_02 successfully takes over and moves, but SH_03+ fail to follow the new leader's path. Currently under investigation.

Drone Recognition (YOLO)
🟡 IN PROGRESS
Awaiting official Gazebo Drone models. Workaround: Currently using the COCO "frisbee" (ID: 74) to test aerial tracking logic.

Multi-Terrain Stress Tests
🟡 IN PROGRESS
Empty and Obstacle worlds verified. Hill/Slope and the high-speed Route-66 map tests are pending.

### Developer Testing Guidelines & Recommendations

Before running full-scale Swarm operations, please read the following testing protocols based on current code stability:

1. Progressive Scaling:
Full hardware and CPU optimization for a 7-robot swarm running simultaneous EKF, Nav2, and LiDAR is not yet finalized.
    • Recommendation: Begin testing with 1 Leader + 1 Follower (num_robots:=2) in the empty_world.sdf.
    • Gradually increase the robot count and introduce obstacles only after confirming base stability on your specific hardware.
   
2. Perception Testing Environment:
The YOLOv8 tracking, Gimbal PID, and Sensor Fusion have been heavily validated in empty_world.sdf.
    • Note: We have not yet run deep perception tests inside dense obstacle environments. Dense obstacles may occasionally trigger the LiDAR "Radar Snap" (false positives).
   
3. The Succession Test (Kill Command):
If you use the ros2 run skyhunter_nav_tools terminate_leader command, be aware of the known bug documented above (Followers > 02 stop moving). A fix for the virtual-topic routing is scheduled for the next commit.


## 1. System Requirements & Setup
OS & Environment

• OS: Ubuntu 22.04

• ROS 2: Humble Hawksbill

• Simulator: Gazebo Harmonic (gz-sim)
    
Install Dependencies
Run the following in your workspace src/ directory:

# Update repositories and install core packages
    sudo apt update && sudo apt install -y \
        ros-humble-nav2-bringup \
        ros-humble-nav2-smac-planner \
        ros-humble-robot-localization \
        ros-humble-pointcloud-to-laserscan \
        ros-humble-ros-gz \
        ros-humble-xacro \
        ros-humble-cv-bridge \
        ros-humble-pcl-conversions \
        libpcl-all-dev \
        libonnxruntime-dev

# Install workspace dependencies
    rosdep install --from-paths src --ignore-src -r -y

## ADDITION : The "Heavy Assets" Download

### Heavy Assets & GPU Drivers Download

**Download Link:** https://drive.google.com/drive/folders/1P1jPJD44KxbKiv8_DyLlt7rTXp7j7Z4w?usp=sharing

**Extraction Instructions:**
After downloading the `.zip` file, extract the folders into your workspace exactly as follows:

      1. Move `onnxruntime_gpu/` to `src/skyhunter_perception/include/`
      2. Move `yolov8m.onnx` to `src/skyhunter_perception/models/`
      3. Move `route_66/` (mesh folder) to `src/skyhunter_gazebo/models/`


*Note: The system will automatically fallback to CPU inference if the ONNX GPU libraries are missing, but latency will drop from 6ms to ~150ms.*

### Build

    cd <your_workspace>
    colcon build 
    source install/setup.bash
    
## 2. Operating the Swarm
Launching the Swarm

The full_swarm.launch.py handles the leader node, Nav2 stack, all followers, communication nodes, and the swarm monitor automatically.

Option A: Empty World (Default)

    ros2 launch skyhunter_bringup full_swarm.launch.py num_robots:=3

Option B: Route 66 World

    ros2 launch skyhunter_bringup full_swarm.launch.py num_robots:=3 world:=route_66.sdf pose:="2400.0 293.5 53.2 0 0 -3.023"

#### LiDAR Modes
You can adjust simulation performance by modifying the LiDAR resolution via the lidar_mode argument:

    • full: Default, high-resolution for leader.
    • half: Balanced, recommended for followers.
    • low: Minimal load, for high-count swarm testing.
    • none: Disables LiDAR.
    
## 3. Swarm Operations & Mission Control

#### Swarm Management
• Terminate Leader (Simulate Hardware Failure):
This triggers the leadership election FSM. Robot 02 will automatically assume the role of leader and re-route the mission.

    ros2 run skyhunter_nav_tools terminate_leader
    
#### Perception & AI
• View YOLO Detections:

To view raw processed detection output:
    
    ros2 run rqt_image_view rqt_image_view
    
#### In the GUI, select topic: /SH_02/perception/overlay

## 4. Package Overview

skyhunter_control
Core logic: leader_node, follower_node, leadership_manager, swarm_monitor.

skyhunter_perception
YOLOv8 engine, ByteTrack, and RCWS fire-solution logic.

skyhunter_comm
WiFi6 RF simulation, distance attenuation, and jamming services.

skyhunter_gazebo
URDF, world files, and bridge configurations.

skyhunter_nav_tools
Mission utility scripts (waypoint_sender, swarm_chaos).

skyhunter_navigation
Nav2 costmap/planner configurations and EKF fusion.

## 5. Network & Comm Architecture
system bonds three layers for reliability:

    1. WiFi6 Mesh (Primary): Ad-hoc 802.11s. Supports LeaderState broadcast at 20 Hz.
    2. LTE (Backup): Failover trigger if RSSI < -75dBm for 3s.
    3. LoRa (Emergency): Heartbeat + GPS only (1 Hz) for critical stop commands if everything else fails.
