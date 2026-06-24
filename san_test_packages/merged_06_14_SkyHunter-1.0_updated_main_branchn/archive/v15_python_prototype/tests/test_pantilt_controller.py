"""Tests for control.pantilt_controller."""
import pytest

from control.pantilt_controller import (
    PanTiltController,
    sweep_speed_dps,
)
from core.messages import (
    PAN_TILT_MAX_SPEED_DPS,
    PAN_TILT_MODE_ENGAGE,
    PAN_TILT_MODE_FIXED,
    PAN_TILT_MODE_SWEEP,
    PAN_TILT_MODE_TRACK,
    SECTOR_MODE_FIXED,
    SECTOR_MODE_SWEEP,
    SECTOR_MODE_TRACK,
    PanTiltCommand,
    SectorAssign,
)

# ─── sweep_speed_dps formula ────────────────────────────────────────────

def test_sweep_speed_zero_when_sector_fits_hfov():
    # 60° HFOV, 60° sector → no sweeping needed.
    assert sweep_speed_dps(60.0, period_sec=10, hfov_deg=60.0) == 0.0


def test_sweep_speed_zero_when_sector_smaller_than_hfov():
    assert sweep_speed_dps(45.0, period_sec=10, hfov_deg=60.0) == 0.0


def test_sweep_speed_formula_uncovered_two_passes_per_period():
    # 90° sector, 60° HFOV → uncovered 30°; two passes / 10 s = 6 °/s.
    assert sweep_speed_dps(90.0, period_sec=10, hfov_deg=60.0) \
        == pytest.approx(6.0)


def test_sweep_speed_180_sector_in_10s():
    # 180° sector, 60° HFOV → uncovered 120°; 240 / 10 = 24 °/s.
    assert sweep_speed_dps(180.0, period_sec=10, hfov_deg=60.0) \
        == pytest.approx(24.0)


def test_sweep_speed_clamped_to_max():
    # Pathologically short period → would exceed gimbal limit; clamp.
    speed = sweep_speed_dps(360.0, period_sec=1, hfov_deg=60.0,
                            max_speed_dps=PAN_TILT_MAX_SPEED_DPS)
    assert speed == PAN_TILT_MAX_SPEED_DPS


def test_sweep_speed_uses_minimum_period_when_zero():
    # period_sec=0 would divide by zero — clamp to 1 internally.
    # (60+60=120 uncovered means... wait, 60 - 60 = 0, but use bigger sector)
    speed = sweep_speed_dps(90.0, period_sec=0, hfov_deg=60.0)
    # uncovered=30, period clamped to 1 → 60 °/s, then clamped to MAX.
    assert speed == pytest.approx(PAN_TILT_MAX_SPEED_DPS)


# ─── PanTiltController.on_sector ────────────────────────────────────────

def _sector_for(robot_id, start, end, mode_hint=SECTOR_MODE_SWEEP,
                valid_period_sec=10):
    return SectorAssign(
        sequence=1,
        robot_id=robot_id,
        sector_start_deg=start,
        sector_end_deg=end,
        valid_period_sec=valid_period_sec,
        mode_hint=mode_hint,
        timestamp_ms=0,
    )


def test_on_sector_ignores_other_robot():
    c = PanTiltController(robot_id=5)
    cmd = c.on_sector(_sector_for(robot_id=6, start=-30, end=+30))
    assert cmd is None
    assert c.last_command is None


def test_on_sector_narrow_sector_parks_fixed():
    # 60° sector == HFOV → fixed mode, speed 0.
    c = PanTiltController(robot_id=5, hfov_deg=60.0)
    cmd = c.on_sector(_sector_for(5, -30.0, +30.0))
    assert cmd.mode == PAN_TILT_MODE_FIXED
    assert cmd.speed_dps == 0.0
    assert cmd.target_pan_deg == pytest.approx(0.0)


def test_on_sector_wide_sector_sweeps():
    # 90° sector → sweep mode with computed speed.
    c = PanTiltController(robot_id=5, hfov_deg=60.0)
    cmd = c.on_sector(_sector_for(5, -45.0, +45.0))
    assert cmd.mode == PAN_TILT_MODE_SWEEP
    assert cmd.speed_dps > 0
    assert cmd.target_pan_deg == pytest.approx(0.0)
    assert cmd.sweep_range_deg == pytest.approx(30.0)  # 90 - 60


def test_on_sector_hub_wrap_around_centered_on_rear():
    # Hub sector (+150, -150) wraps through ±180. The pan-tilt center
    # should land at rear (180° or -180°).
    c = PanTiltController(robot_id=8, hfov_deg=60.0)
    cmd = c.on_sector(_sector_for(8, +150.0, -150.0))
    # 60° wrap sector ≤ HFOV → fixed mode.
    assert cmd.mode == PAN_TILT_MODE_FIXED
    # 180 and -180 are equivalent; accept either after normalization.
    pan_norm = (cmd.target_pan_deg + 360) % 360
    assert pan_norm == pytest.approx(180.0) or pan_norm == pytest.approx(0.0)


def test_on_sector_track_hint_without_target_falls_back_to_sweep():
    # mode_hint=track on a wide sector but no AI track yet → sweep until
    # on_track() supplies a bearing.
    c = PanTiltController(robot_id=5, hfov_deg=60.0)
    cmd = c.on_sector(_sector_for(
        5, -90.0, +90.0, mode_hint=SECTOR_MODE_TRACK))
    assert cmd.mode == PAN_TILT_MODE_SWEEP


def test_on_sector_fixed_hint_overrides_sweep_speed():
    # SECTOR_MODE_FIXED on a wide sector → controller still parks since
    # fixed has no sweep semantics.
    c = PanTiltController(robot_id=5, hfov_deg=60.0)
    cmd = c.on_sector(_sector_for(
        5, -90.0, +90.0, mode_hint=SECTOR_MODE_FIXED))
    assert cmd.mode == PAN_TILT_MODE_FIXED
    assert cmd.speed_dps == 0.0


def test_on_sector_caches_last_command():
    c = PanTiltController(robot_id=5)
    cmd = c.on_sector(_sector_for(5, -45, +45))
    assert c.last_command is cmd


def test_on_sector_command_validates():
    c = PanTiltController(robot_id=5)
    cmd = c.on_sector(_sector_for(5, -60, +60))
    # Should not raise; envelope is enforced.
    cmd.validate()


# ─── PanTiltController.on_track ─────────────────────────────────────────

def test_on_track_targets_bearing_and_sets_track_mode():
    c = PanTiltController(robot_id=5)
    cmd = c.on_track(bearing_deg=+45.0, now_ms=1234)
    assert cmd.mode == PAN_TILT_MODE_TRACK
    assert cmd.target_pan_deg == pytest.approx(45.0)
    assert cmd.timestamp_ms == 1234


def test_on_track_clamps_speed_to_max():
    c = PanTiltController(robot_id=5)
    cmd = c.on_track(bearing_deg=0.0, speed_dps=1000.0)
    assert cmd.speed_dps == PAN_TILT_MAX_SPEED_DPS


def test_on_track_clamps_pan_to_hardware_envelope():
    c = PanTiltController(robot_id=5)
    cmd = c.on_track(bearing_deg=+250.0)
    # Clamped to +180.
    assert cmd.target_pan_deg == 180.0


def test_on_track_respects_custom_tilt():
    c = PanTiltController(robot_id=5, default_tilt_deg=0.0)
    cmd = c.on_track(bearing_deg=0.0, tilt_deg=+30.0)
    assert cmd.target_tilt_deg == pytest.approx(30.0)


def test_on_track_updates_last_command():
    c = PanTiltController(robot_id=5)
    sweep = c.on_sector(_sector_for(5, -90, +90))
    track = c.on_track(bearing_deg=+30.0)
    assert c.last_command is track
    assert c.last_command is not sweep


# ─── sequence numbering (v1.1 schema addition) ──────────────────────────

def test_sequence_starts_at_one():
    c = PanTiltController(robot_id=5)
    cmd = c.on_sector(_sector_for(5, -30, +30))
    assert cmd.sequence == 1


def test_sequence_increments_per_command():
    c = PanTiltController(robot_id=5)
    a = c.on_sector(_sector_for(5, -30, +30))
    b = c.on_track(bearing_deg=10.0)
    cc = c.on_sector(_sector_for(5, -45, +45))
    assert [a.sequence, b.sequence, cc.sequence] == [1, 2, 3]


def test_sequence_skipped_when_message_targets_other_robot():
    # on_sector returning None must not consume a sequence number.
    c = PanTiltController(robot_id=5)
    assert c.on_sector(_sector_for(robot_id=99, start=-30, end=+30)) is None
    next_cmd = c.on_sector(_sector_for(5, -30, +30))
    assert next_cmd.sequence == 1


# ─── engage mode (v1.1 schema addition) ─────────────────────────────────

def test_on_engage_sets_engage_mode():
    c = PanTiltController(robot_id=5)
    cmd = c.on_engage(bearing_deg=+45.0)
    assert cmd.mode == PAN_TILT_MODE_ENGAGE
    assert cmd.target_pan_deg == pytest.approx(45.0)


def test_on_engage_clamps_speed_to_max():
    c = PanTiltController(robot_id=5)
    cmd = c.on_engage(bearing_deg=0.0, speed_dps=9999.0)
    assert cmd.speed_dps == PAN_TILT_MAX_SPEED_DPS


def test_on_engage_validates():
    c = PanTiltController(robot_id=5)
    cmd = c.on_engage(bearing_deg=-90.0, tilt_deg=15.0)
    cmd.validate()
    assert cmd.target_tilt_deg == pytest.approx(15.0)


def test_pantilt_command_engage_mode_validates():
    PanTiltCommand(mode=PAN_TILT_MODE_ENGAGE).validate()
