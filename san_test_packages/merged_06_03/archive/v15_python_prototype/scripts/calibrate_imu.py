"""IMU boresight calibration — gravity alignment + ZUPT bias (P2-12).

Procedure:
1. Place robot on level surface, stationary
2. Capture 30 s of IMU data
3. Compute gravity vector → orientation (boresight roll/pitch)
4. ZUPT (Zero Velocity Update): gyroscope bias when stationary
5. Output /etc/patrol/calibration/imu.yaml
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import List, Tuple

import numpy as np
import yaml


def collect_samples(duration_s: float = 30.0,
                    sample_rate_hz: float = 100.0
                    ) -> List[Tuple[np.ndarray, np.ndarray]]:
    """Collect (accel, gyro) samples from IMU.

    Production: read from /dev/imu0 or ROS topic.
    Stub: synthetic data — gravity along -z, small noise + small bias.
    """
    n_samples = int(duration_s * sample_rate_hz)
    rng = np.random.default_rng(seed=42)
    accel_samples = rng.normal(
        loc=[0.0, 0.0, -9.81], scale=0.05, size=(n_samples, 3))
    gyro_samples = rng.normal(
        loc=[0.001, -0.002, 0.0005], scale=0.001, size=(n_samples, 3))
    return list(zip(accel_samples, gyro_samples, strict=True))


def compute_calibration(samples) -> dict:
    """From stationary IMU samples, compute boresight + bias."""
    accels = np.array([s[0] for s in samples])
    gyros = np.array([s[1] for s in samples])

    mean_accel = accels.mean(axis=0)
    mean_gyro = gyros.mean(axis=0)

    gravity_norm = float(np.linalg.norm(mean_accel))
    if gravity_norm < 9.0 or gravity_norm > 10.5:
        raise ValueError(
            f"Implausible gravity {gravity_norm:.2f} m/s² — recalibrate")

    # The accelerometer measures specific force = a − g, so at rest it reads
    # the negation of gravity. The "down" direction in the body frame is
    # therefore -mean_accel; use that to recover roll/pitch so that a level
    # robot reads 0/0 (not 180°/0°).
    g_vec_unit = -mean_accel / gravity_norm
    pitch_rad = float(np.arctan2(
        -g_vec_unit[0],
        np.sqrt(g_vec_unit[1] ** 2 + g_vec_unit[2] ** 2)))
    roll_rad = float(np.arctan2(g_vec_unit[1], g_vec_unit[2]))

    return {
        "calibration_ts": time.time(),
        "imu_serial": "stub_imu_001",
        "gravity_norm_mps2": gravity_norm,
        "boresight": {
            "roll_deg": float(np.degrees(roll_rad)),
            "pitch_deg": float(np.degrees(pitch_rad)),
            "yaw_deg": 0.0,
        },
        "gyro_bias_rad_s": {
            "x": float(mean_gyro[0]),
            "y": float(mean_gyro[1]),
            "z": float(mean_gyro[2]),
        },
        "noise_estimate": {
            "accel_std_mps2": float(accels.std(axis=0).mean()),
            "gyro_std_rad_s": float(gyros.std(axis=0).mean()),
        },
        "sample_count": len(samples),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=30.0,
                        help="capture duration (s)")
    parser.add_argument("--rate", type=float, default=100.0,
                        help="sample rate (Hz)")
    parser.add_argument("--output", type=str,
                        default="/etc/patrol/calibration/imu.yaml")
    args = parser.parse_args()

    print(f"[1/3] Collecting {args.duration}s of IMU data at {args.rate}Hz...")
    print("      KEEP ROBOT STATIONARY ON LEVEL SURFACE")
    samples = collect_samples(duration_s=args.duration,
                              sample_rate_hz=args.rate)
    print(f"      Captured {len(samples)} samples")

    print("[2/3] Computing calibration...")
    try:
        cal = compute_calibration(samples)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"[3/3] Writing {out_path}...")
    with out_path.open("w") as f:
        yaml.dump(cal, f, default_flow_style=False)

    print("Done.")
    print(f"  Pitch: {cal['boresight']['pitch_deg']:.3f}°")
    print(f"  Roll:  {cal['boresight']['roll_deg']:.3f}°")
    print(f"  Gyro bias: {cal['gyro_bias_rad_s']}")


if __name__ == "__main__":
    main()
