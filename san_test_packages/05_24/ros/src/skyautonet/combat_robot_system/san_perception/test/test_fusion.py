"""SAN v1.5 Phase 2-E Turn 11-12 — Fusion math tests.

Coverage:
   F1  CameraCalib.K matrix layout
   F2  project_bbox identity transform (same cam, no extrinsic offset)
   F3  project_bbox with depth scaling
   F4  project_bbox returns None for depth<=0
   F5  project_bbox returns None for invalid bbox
   F6  thermal_stats valid region
   F7  thermal_stats out-of-bounds → invalid
   F8  thermal_stats empty bbox → invalid
"""
import os
import sys

sys.path.insert(
    0,
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
)

import numpy as np
from san_perception.fusion import (
    CameraCalib,
    StereoExtrinsic,
    project_bbox_rgb_to_thermal,
    thermal_stats_in_bbox,
)

# ─── Camera calibration ─────────────────────────────────────────────────

def test_f1_camera_calib_k_matrix():
    cal = CameraCalib(width=1920, height=1080,
                       fx=1500.0, fy=1500.0,
                       cx=960.0, cy=540.0)
    K = cal.K
    assert K.shape == (3, 3)
    assert K[0, 0] == 1500.0
    assert K[1, 1] == 1500.0
    assert K[0, 2] == 960.0
    assert K[1, 2] == 540.0
    assert K[2, 2] == 1.0


# ─── Projection ─────────────────────────────────────────────────────────

def test_f2_project_identity_transform():
    # Same camera intrinsics, identity extrinsic → bbox approximately unchanged
    cal = CameraCalib(width=640, height=480,
                       fx=500.0, fy=500.0, cx=320.0, cy=240.0)
    ext = StereoExtrinsic()                         # R=I, t=0
    bbox = (100, 100, 200, 200)
    out = project_bbox_rgb_to_thermal(
        bbox, depth_m=5.0, rgb=cal, thermal=cal, ext=ext)
    assert out is not None
    # Within ±1 pixel of original (floor/ceil rounding)
    assert abs(out[0] - 100) <= 1
    assert abs(out[1] - 100) <= 1
    assert abs(out[2] - 200) <= 1
    assert abs(out[3] - 200) <= 1


def test_f3_project_with_baseline_offset():
    # X-baseline of 10 cm at 5 m depth, fx=500 → 10 px shift left
    # in thermal frame.
    cal = CameraCalib(width=640, height=480,
                       fx=500.0, fy=500.0, cx=320.0, cy=240.0)
    ext = StereoExtrinsic()
    ext.t = np.array([0.1, 0.0, 0.0])     # 10 cm baseline
    bbox = (300, 220, 340, 260)
    out = project_bbox_rgb_to_thermal(
        bbox, depth_m=5.0, rgb=cal, thermal=cal, ext=ext)
    assert out is not None
    # tx1 should be shifted right by ~10px (positive t shifts projection +x)
    assert out[0] >= 309
    assert out[0] <= 311


def test_f4_project_zero_depth():
    cal = CameraCalib(width=640, height=480,
                       fx=500.0, fy=500.0, cx=320.0, cy=240.0)
    out = project_bbox_rgb_to_thermal(
        (100, 100, 200, 200), depth_m=0.0,
        rgb=cal, thermal=cal, ext=StereoExtrinsic())
    assert out is None

    out2 = project_bbox_rgb_to_thermal(
        (100, 100, 200, 200), depth_m=-1.0,
        rgb=cal, thermal=cal, ext=StereoExtrinsic())
    assert out2 is None


def test_f5_project_invalid_bbox():
    cal = CameraCalib(width=640, height=480,
                       fx=500.0, fy=500.0, cx=320.0, cy=240.0)
    # x2 <= x1
    out = project_bbox_rgb_to_thermal(
        (200, 100, 100, 200), depth_m=5.0,
        rgb=cal, thermal=cal, ext=StereoExtrinsic())
    assert out is None


# ─── Thermal stats ──────────────────────────────────────────────────────

def test_f6_thermal_stats_valid():
    # 100×100 image with all values = 27315 (= 0 °C in centikelvin)
    img = np.full((100, 100), 27315, dtype=np.uint16)
    img[50:70, 50:70] = 30315          # +30 °C patch
    stats = thermal_stats_in_bbox(img, (50, 50, 70, 70))
    assert stats.valid
    assert abs(stats.mean_c - 30.0) < 1e-6
    assert abs(stats.max_c - 30.0) < 1e-6


def test_f7_thermal_stats_out_of_bounds():
    img = np.zeros((100, 100), dtype=np.uint16)
    # Bbox at (200, 200, 300, 300) — entirely outside
    stats = thermal_stats_in_bbox(img, (200, 200, 300, 300))
    assert not stats.valid


def test_f8_thermal_stats_empty_bbox():
    img = np.zeros((100, 100), dtype=np.uint16)
    stats = thermal_stats_in_bbox(img, (50, 50, 50, 50))   # zero-area
    assert not stats.valid
