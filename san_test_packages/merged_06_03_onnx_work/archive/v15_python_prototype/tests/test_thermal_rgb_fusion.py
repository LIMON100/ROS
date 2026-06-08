"""
Tests for thermal-RGB fusion.

Verifies:
  • TimeSyncBuffer returns nearest within tolerance, None outside
  • project_bbox: identity calibration → bbox preserved
  • project_bbox: stereo offset shifts the projection
  • thermal_stats: min/mean/max within expected range
  • End-to-end: enrich() returns sensible payload
"""
from __future__ import annotations

import numpy as np
import pytest

from perception.thermal_rgb_fusion import (
    CameraCalib,
    FuserConfig,
    StereoExtrinsic,
    ThermalRgbFuser,
    TimeSyncBuffer,
    project_bbox_rgb_to_thermal,
    thermal_stats_in_bbox,
)


# ────────── TimeSyncBuffer ──────────
def test_sync_returns_nearest_within_tolerance():
    buf = TimeSyncBuffer()
    buf.add(100.00, "a")
    buf.add(100.05, "b")
    buf.add(100.10, "c")
    out = buf.nearest(100.06, max_dt_s=0.05)
    assert out is not None
    stamp, data = out
    assert data == "b"


def test_sync_returns_none_when_no_match_within_tolerance():
    buf = TimeSyncBuffer()
    buf.add(100.0, "a")
    assert buf.nearest(105.0, max_dt_s=0.5) is None


def test_sync_empty_buffer_returns_none():
    assert TimeSyncBuffer().nearest(100.0) is None


def test_sync_evicts_oldest_when_full():
    buf = TimeSyncBuffer(max_size=3)
    for i, t in enumerate([10.0, 11.0, 12.0, 13.0]):
        buf.add(t, f"f{i}")
    assert len(buf) == 3
    # Oldest (t=10) should have been dropped — 10.0 query falls back to 11.0
    out = buf.nearest(10.0, max_dt_s=2.0)
    assert out[1] == "f1"     # t=11.0 was kept


def test_sync_ignores_out_of_order_arrivals():
    """Real-time pipeline: stale frames arriving after newer ones are dropped."""
    buf = TimeSyncBuffer()
    buf.add(100.0, "fresh")
    buf.add(99.0, "stale")    # ignored
    out = buf.nearest(99.0, max_dt_s=2.0)
    assert out[1] == "fresh"


# ────────── Projection ──────────
def _identical_calib(side: int = 640) -> CameraCalib:
    return CameraCalib(width=side, height=side,
                       fx=500.0, fy=500.0, cx=side / 2, cy=side / 2)


def test_projection_identity_passes_bbox_through():
    """Same intrinsics + zero extrinsic → projected bbox equals input."""
    rgb = _identical_calib()
    therm = _identical_calib()
    ext = StereoExtrinsic()    # identity
    bbox = (100, 200, 300, 400)
    out = project_bbox_rgb_to_thermal(bbox, depth_m=10.0,
                                      rgb=rgb, thermal=therm, ext=ext)
    assert out is not None
    # With identical calibration, output ≈ input (one pixel of rounding)
    for a, b in zip(out, bbox, strict=False):
        assert abs(a - b) <= 1


def test_projection_with_stereo_offset_shifts_horizontally():
    """5 cm baseline along +x at 5 m depth → ~5 pixel shift."""
    rgb = _identical_calib()
    therm = _identical_calib()
    ext = StereoExtrinsic(t=np.array([-0.05, 0, 0]))   # thermal is 5cm to the LEFT
    bbox_in = (300, 200, 400, 300)
    out = project_bbox_rgb_to_thermal(bbox_in, depth_m=5.0,
                                      rgb=rgb, thermal=therm, ext=ext)
    assert out is not None
    # Subject 5 m away, baseline 0.05 m, fx 500 → shift ≈ -0.05*500/5 = -5 px
    expected_shift = int(round(-0.05 * 500 / 5.0))
    actual_shift = out[0] - bbox_in[0]
    assert abs(actual_shift - expected_shift) <= 1


def test_projection_returns_none_when_bbox_falls_outside_thermal_fov():
    """Project with a small thermal sensor + far-off RGB bbox."""
    rgb = CameraCalib(width=3840, height=2160, fx=2000, fy=2000,
                      cx=1920, cy=1080)
    # Tiny thermal sensor so the projection escapes its bounds
    therm = CameraCalib(width=64, height=64, fx=200, fy=200, cx=32, cy=32)
    ext = StereoExtrinsic()
    # Extreme corner of the RGB image
    bbox = (3700, 2050, 3800, 2150)
    out = project_bbox_rgb_to_thermal(bbox, depth_m=10.0,
                                      rgb=rgb, thermal=therm, ext=ext)
    assert out is None


def test_projection_zero_depth_returns_none():
    rgb = _identical_calib()
    therm = _identical_calib()
    out = project_bbox_rgb_to_thermal((10, 10, 50, 50), depth_m=0.0,
                                      rgb=rgb, thermal=therm,
                                      ext=StereoExtrinsic())
    assert out is None


# ────────── Thermal stats ──────────
def test_thermal_stats_uniform_region():
    """Uniform thermal pixels should yield min == mean == max."""
    frame = np.full((100, 100), 32768, dtype=np.uint16)   # midpoint
    out = thermal_stats_in_bbox(frame, (10, 10, 50, 50),
                                min_temp_c=-20.0, max_temp_c=120.0)
    assert out is not None
    # raw 32768 → 50% of [-20, 120] = 50°C
    assert out["min_c"] == pytest.approx(50.0, abs=0.5)
    assert out["mean_c"] == pytest.approx(50.0, abs=0.5)
    assert out["max_c"] == pytest.approx(50.0, abs=0.5)
    assert out["n_px"] == 40 * 40


def test_thermal_stats_finds_hot_spot():
    frame = np.full((100, 100), 1000, dtype=np.uint16)    # ~cold
    frame[40:50, 40:50] = 60000                           # very hot patch
    out = thermal_stats_in_bbox(frame, (35, 35, 55, 55),
                                min_temp_c=-20.0, max_temp_c=120.0)
    assert out is not None
    # Max should reflect the 60000 raw value → ~108°C
    assert out["max_c"] > 100.0
    # Min should be near the cold value
    assert out["min_c"] < 0.0


def test_thermal_stats_invalid_region_returns_none():
    frame = np.zeros((10, 10), dtype=np.uint16)
    # Inverted bbox
    assert thermal_stats_in_bbox(frame, (5, 5, 5, 5),
                                  min_temp_c=0, max_temp_c=100) is None


# ────────── End-to-end fuser ──────────
def test_fuser_enrich_returns_temperature_and_sync_dt():
    cfg = FuserConfig(
        rgb_calib=_identical_calib(),
        thermal_calib=_identical_calib(),
        extrinsic=StereoExtrinsic(),
        sync_tolerance_s=0.10,
    )
    fuser = ThermalRgbFuser(cfg)
    # Plant a thermal frame with a localized hot spot
    therm = np.full((640, 640), 1000, dtype=np.uint16)
    therm[200:250, 100:150] = 50000
    fuser.add_thermal_frame(stamp=100.00, mono16=therm)

    out = fuser.enrich(rgb_stamp=100.05, bbox_rgb=(100, 200, 150, 250),
                        depth_m=10.0)
    assert out is not None
    # sync_dt should be ≈ -0.05 (thermal is 50ms older than RGB query)
    assert abs(out["sync_dt_s"] + 0.05) < 0.01
    assert out["max_c"] > 50.0       # captured the hot spot
    assert fuser.stats["ok"] == 1


def test_fuser_returns_none_when_no_thermal_frame_available():
    cfg = FuserConfig(
        rgb_calib=_identical_calib(),
        thermal_calib=_identical_calib(),
    )
    fuser = ThermalRgbFuser(cfg)
    out = fuser.enrich(rgb_stamp=100.0, bbox_rgb=(0, 0, 100, 100), depth_m=5.0)
    assert out is None
    assert fuser.stats["sync_miss"] == 1


def test_fuser_returns_none_when_thermal_too_old():
    cfg = FuserConfig(
        rgb_calib=_identical_calib(),
        thermal_calib=_identical_calib(),
        sync_tolerance_s=0.05,   # tight tolerance
    )
    fuser = ThermalRgbFuser(cfg)
    fuser.add_thermal_frame(100.00, np.zeros((640, 640), dtype=np.uint16))
    # RGB is 200 ms after thermal → outside tolerance
    out = fuser.enrich(rgb_stamp=100.20, bbox_rgb=(0, 0, 100, 100),
                       depth_m=5.0)
    assert out is None
    assert fuser.stats["sync_miss"] == 1
