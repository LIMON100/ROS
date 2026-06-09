# LiDAR configuration

SAN-SDD-SWARM-001 v1.1 §4.

## Sensor matrix

| Platform | Sensor | Driver | Config |
|----------|--------|--------|--------|
| Leader (Go2) | Unitree L1 (internal) | `unitree_lidar_ros2` | upstream defaults |
| Follower | Robosense E1 | `rslidar_sdk` | `rslidar_e1.yaml` |
| Hub UGV | Robosense E1 | `rslidar_sdk` | `rslidar_e1.yaml` |

The Leader keeps the Unitree L1 because the Go2 platform integrates it
into the chassis and exposes the topic through the Unitree SDK. The
Follower + Hub UGV mount the externally-procured Robosense E1 for the
v1.1 spec's 200 m / 120° FoV target.

## Files

- `rslidar_e1.yaml` — Robosense E1 driver config (UDP ports, FoV
  bounds, distance limits).
- `slam_toolbox_e1.yaml` — slam_toolbox tuning that matches the E1's
  range + point density. Replaces the v1.0 Unitree-L1 preset on every
  robot that switched to the E1.

## Blind-spot compensation

The E1's 120° HFOV means the rear + side-rear are unseen. A robot
reversing toward a cliff would never get a point-cloud return from
it. **`safety/cliff_detector.py`** runs an IMU-pitch monitor that
emits an `EmergencyAlert(CLIFF_DETECTED)` when the chassis tilts past
15° for ≥2 consecutive ticks. This is a backup gate, not a replacement
for cautious path planning — operator review of high-speed reverse
manoeuvres is still required.

## Installation (production hardware)

```bash
sudo apt install ros-humble-rslidar-sdk
# or from source:
git clone https://github.com/RoboSense-LiDAR/rslidar_sdk.git \
    -b dev_opensource
```

The CI environment does not install the driver — the Python codebase
only consumes point clouds via the existing `lidar_ref` queue shape,
which is sensor-agnostic.
