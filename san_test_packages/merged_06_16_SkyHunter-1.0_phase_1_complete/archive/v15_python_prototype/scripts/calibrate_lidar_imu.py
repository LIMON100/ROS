"""LiDAR-IMU extrinsic calibration — hand-eye (P2-12).

Procedure:
1. Move robot through 3-axis rotations (lazy figure-8)
2. Solve hand-eye AX = XB for X (LiDAR frame in IMU frame)
3. Output /etc/patrol/calibration/lidar_imu.yaml

Production: kalibr or lidar_align. This script is a stub that emits a
plausible-looking yaml so downstream consumers (and CI) can exercise the
end-to-end flow without bag data.
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import yaml


def stub_solve_extrinsic() -> dict:
    """Identity rotation + 5 cm vertical offset (sensor stacked over IMU)."""
    return {
        "calibration_ts": time.time(),
        "method": "stub_hand_eye",
        "translation_m": [0.0, 0.0, 0.05],
        "rotation_quaternion": [0.0, 0.0, 0.0, 1.0],
        "rms_error_deg": 0.3,
        "rms_error_m": 0.005,
        "sample_count": 200,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bag-file", default="",
                        help="ROS bag with /imu and /lidar")
    parser.add_argument("--output",
                        default="/etc/patrol/calibration/lidar_imu.yaml")
    args = parser.parse_args()

    print("[stub] Hand-eye calibration -- full impl uses kalibr/lidar_align")
    cal = stub_solve_extrinsic()
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w") as f:
        yaml.dump(cal, f, default_flow_style=False)
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
