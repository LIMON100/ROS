# tin3_navigation

Navigation and localization configs for TIN3 robot.

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

## Usage

```bash
# EKF only (sensor fusion)
ros2 launch tin3_navigation ekf.launch.py

# Full navigation stack
ros2 launch tin3_navigation nav2.launch.py

# With namespace (multi-robot)
ros2 launch tin3_navigation ekf.launch.py robot_ns:=robot_01
```

## Dependencies

- `tin3_description` - Robot URDF
- `robot_localization` - EKF sensor fusion
- `nav2_bringup` - Navigation stack

## Notes

- Works with both simulation and real robot
- Same configs used in sim and real (use_sim_time param differs)
