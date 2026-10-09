# Vendored: Unitree Go2 SITL (`unitree_go2_ros2` + CHAMP)

This tree is a **vendored third-party dependency**, committed into the SkyHunter
repo so the Go2 SITL convoy demo (`san_sim_gazebo/launch/convoy_demo.launch.py`)
is self-contained — no external clone needed.

## Upstream

- Source: `khaledgabr77/unitree_go2_ros2` — ROS 2 Jazzy integration of the Unitree
  Go2 quadruped using the CHAMP controller framework (Gazebo Harmonic).
- Vendored: 2026-06-20 (working tree, `.git` and `*.bak`/`*.sensorbak` excluded).

## Licensing

| Package | License |
|---|---|
| `champ`, `champ_base`, `champ_msgs` | **BSD** (CHAMP, `chvmp/champ`) — see `champ/include/champ/LICENSE` |
| `unitree_go2_description`, `unitree_go2_sim` | Upstream `package.xml` declares `TODO: License declaration` (**undeclared**) |
| Robot meshes (`unitree_go2_description/meshes/*.dae`) | © **Unitree Robotics** robot model assets |

⚠ The `unitree_go2_*` packages and Unitree mesh assets carry no explicit upstream
license. They are vendored **for internal R&D simulation use only**; clear
licensing terms must be confirmed before any redistribution.

## Large binaries

The robot meshes exceed the repo `check-added-large-files` 500 KB limit
(`trunk.dae` ≈ 10.7 MB, total meshes ≈ 30 MB). `^ros/src/third_party/` is excluded
from all pre-commit hooks (see `.pre-commit-config.yaml`) so the meshes commit
directly and upstream sources stay pristine.

## SkyHunter modifications (vs upstream)

Applied for the convoy demo; the working tree already contains these:

1. **`unitree_go2_description/urdf/unitree_go2_gazebo.xacro`** — odom plugin topic
   `/odom` → `/odom_gt` (separate ground-truth leader odom for the convoy
   coordinator; keeps the EKF `/odom` chain intact).
2. **`unitree_go2_sim/launch/unitree_go2_launch.py`** — bridge entry `/odom` →
   `/odom_gt` to match (1).
3. **`unitree_go2_sim/config/gait/gait.yaml`** — `max_linear_velocity_x` /
   `max_angular_velocity_z` tuned for the convoy (gentle, low-RTF stable gait).
4. **Sensors disabled for RTF** (`RTF_OFF` / `RTF_OFF_CAM` comment markers in
   `unitree_go2_robot.xacro` + `unitree_go2_gazebo.xacro`): velodyne, 4D LiDAR,
   d455 RGBD includes + the inline rgb camera block. The leader is driven by
   `/cmd_vel` and needs no perception for the demo; removing render sensors keeps
   RTF high so the CHAMP gait stays stable (low RTF → gait instability/fall).
   IMU/NavSat (no render) are kept. **To restore sensors:** remove the `RTF_OFF`
   comment wrappers.
