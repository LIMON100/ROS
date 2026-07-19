"""Tests for calibration tools (P2-12)."""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
DOCS = REPO_ROOT / "doc"


# ───── Existence ─────
def test_scripts_exist():
    for name in ("calibrate_imu.py", "calibrate_camera.py",
                 "calibrate_lidar_imu.py"):
        assert (SCRIPTS / name).exists(), f"missing {name}"


def test_calibration_doc_exists():
    assert (DOCS / "calibration_procedure.md").exists()


# ───── IMU calibration ─────
def test_imu_calibration_produces_yaml():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "imu.yaml"
        r = subprocess.run(
            [sys.executable, str(SCRIPTS / "calibrate_imu.py"),
             "--duration", "1.0", "--rate", "100",
             "--output", str(out)],
            capture_output=True, text=True, timeout=20)
        assert r.returncode == 0, r.stderr
        assert out.exists()
        with out.open(encoding="utf-8") as f:
            cal = yaml.safe_load(f)
        assert "boresight" in cal
        assert "gyro_bias_rad_s" in cal
        assert "gravity_norm_mps2" in cal
        assert 9.0 < cal["gravity_norm_mps2"] < 10.5


def test_imu_compute_calibration_unit():
    """Direct unit test of compute_calibration()."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        from calibrate_imu import collect_samples, compute_calibration
    finally:
        sys.path.pop(0)
    samples = collect_samples(duration_s=1.0, sample_rate_hz=100.0)
    cal = compute_calibration(samples)
    assert "boresight" in cal
    assert "gyro_bias_rad_s" in cal
    assert abs(cal["boresight"]["roll_deg"]) < 5.0
    assert abs(cal["boresight"]["pitch_deg"]) < 5.0


# ───── Camera calibration (stub path) ─────
def test_camera_calibration_stub_mode():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "camera.yaml"
        r = subprocess.run(
            [sys.executable, str(SCRIPTS / "calibrate_camera.py"),
             "--images-dir", "/nonexistent",
             "--output", str(out)],
            capture_output=True, text=True, timeout=20)
        assert r.returncode == 0, r.stderr
        assert out.exists()
        with out.open(encoding="utf-8") as f:
            cal = yaml.safe_load(f)
        assert "K" in cal
        assert len(cal["K"]) == 3 and len(cal["K"][0]) == 3


# ───── LiDAR-IMU stub ─────
def test_lidar_imu_stub_runs():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "lidar_imu.yaml"
        r = subprocess.run(
            [sys.executable, str(SCRIPTS / "calibrate_lidar_imu.py"),
             "--output", str(out)],
            capture_output=True, text=True, timeout=20)
        assert r.returncode == 0, r.stderr
        with out.open(encoding="utf-8") as f:
            cal = yaml.safe_load(f)
        assert "translation_m" in cal
        assert len(cal["translation_m"]) == 3
        assert "rotation_quaternion" in cal
        assert len(cal["rotation_quaternion"]) == 4


# ───── Doc content ─────
def test_doc_includes_required_sections():
    doc = (DOCS / "calibration_procedure.md").read_text(encoding="utf-8")
    for section in ("IMU", "Camera", "LiDAR-IMU", "RTK Base",
                    "Re-calibration"):
        assert section in doc, f"missing section: {section}"


# ───── Implausible gravity → raises ─────
def test_imu_implausible_gravity_raises():
    import numpy as np
    sys.path.insert(0, str(SCRIPTS))
    try:
        from calibrate_imu import compute_calibration
    finally:
        sys.path.pop(0)
    bad = [(np.array([0.0, 0.0, -5.0]),
            np.array([0.0, 0.0, 0.0])) for _ in range(50)]
    with pytest.raises(ValueError, match="Implausible gravity"):
        compute_calibration(bad)
