# Calibration Procedure (P2-12)

Sensor calibration steps for the SAN swarm robot. Run all four scripts in
order. Re-run when any of the triggers in the bottom section fire.

## Prerequisites

- Robot stationary on a flat surface
- Lighting: indoor, no direct sunlight
- Chessboard: 10×7 squares (9×6 inner corners), 25 mm squares, A4 size

## 1. IMU Boresight + ZUPT (5 min)

```bash
python scripts/calibrate_imu.py --duration 30 \
    --output /etc/patrol/calibration/imu.yaml
```

Robot must be **stationary** during the 30 s capture. Tilt < 5° expected.

## 2. Camera Intrinsics (10–20 min)

```bash
# Capture 20+ chessboard images at varied angles
python scripts/capture_chessboard.py --output /tmp/calib_images/

# Compute calibration
python scripts/calibrate_camera.py \
    --images-dir /tmp/calib_images/ \
    --output /etc/patrol/calibration/camera.yaml
```

Acceptance: RMS error < 1.0 px for a 1080p sensor.

## 3. LiDAR-IMU Extrinsic (15 min)

```bash
# Record a bag while moving the robot in a 3-axis rotation pattern
ros2 bag record /imu /scan -o /tmp/calib.bag

# Solve hand-eye
python scripts/calibrate_lidar_imu.py \
    --bag-file /tmp/calib.bag \
    --output /etc/patrol/calibration/lidar_imu.yaml
```

Move the robot through a lazy figure-8 covering all three axes.

## 4. RTK Base Station (1–24 h)

```bash
python scripts/calibrate_rtk_base.py \
    --duration 3600 \
    --output /etc/patrol/calibration/rtk_base.yaml
```

24 h survey-in for sub-cm accuracy; 1 h is adequate for development.

## Validation

After all 4 calibrations:

```bash
python scripts/validate_calibration.py
```

This script verifies:
- All 4 yaml files exist and parse
- Values within plausible ranges
- No NaN or extreme outliers

## Re-calibration Triggers

Re-run when:
- Sensor mount physically moved
- Vibration shock event (drop, collision)
- 6 months elapsed since last calibration
- Quality metrics degrade in field (KPP §2.1.1 violations)
