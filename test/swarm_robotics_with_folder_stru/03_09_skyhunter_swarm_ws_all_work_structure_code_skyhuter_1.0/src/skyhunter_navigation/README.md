# tin3_navigation

Navigation and localization configs for TIN3 robot.

---

## Folder Structure

```
tin3_navigation/
├── launch/
│   ├── ekf.launch.py        # EKF localization only
│   └── nav2.launch.py       # Full Nav2 stack
├── config/
│   ├── ekf.yaml             # robot_localization EKF params
│   └── nav2_params.yaml     # Nav2 navigation params
└── rviz/
    └── navigation.rviz      # Nav2 visualization
```

---

## Usage

```bash
# EKF only (sensor fusion)
ros2 launch tin3_navigation ekf.launch.py

# Full navigation stack
ros2 launch tin3_navigation nav2.launch.py
```

---

## EKF Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 30 Hz |
| Output topic | `/odom_filtered` |
| TF published | `odom → base_footprint` |

**Sensor fusion inputs:**

| Source | Topic | Fused States |
|--------|-------|--------------|
| Wheel odometry | `/odom` | x, y, vx, vyaw |
| IMU | `/imu/data` | roll_vel, pitch_vel, yaw_vel |

---

## Dependencies

- `tin3_description` — robot URDF
- `robot_localization` — EKF sensor fusion
- `nav2_bringup` — navigation stack
- `pointcloud_to_laserscan` — LiDAR point cloud conversion

```bash
# Install dependencies
rosdep install --from-paths src --ignore-src -r -y

# Or manually:
sudo apt install ros-${ROS_DISTRO}-pointcloud-to-laserscan
sudo apt install ros-${ROS_DISTRO}-nav2-bringup ros-${ROS_DISTRO}-nav2-smac-planner
```

---

## Notes

- `use_sim_time: true` is set in `ekf.yaml` for simulation
- Works with both simulation and real robot — set `use_sim_time` accordingly