"""Tests for safety.cliff_detector — IMU-pitch backup for E1 blind spot."""
import math

import numpy as np
import pytest

from core.messages import (
    EMERGENCY_TYPE_CLIFF_DETECTED,
    EmergencyAlert,
    Header,
    ImuData,
)
from safety.cliff_detector import (
    CLIFF_SEVERITY,
    DEFAULT_PITCH_THRESHOLD_DEG,
    CliffDetector,
    quaternion_to_pitch_deg,
)

# ─── EmergencyAlert validators ─────────────────────────────────────────

def test_emergency_alert_defaults_rejected():
    # Default `type=0` isn't a known emergency type; must reject.
    with pytest.raises(ValueError):
        EmergencyAlert().validate()


def test_emergency_alert_cliff_validates():
    EmergencyAlert(
        type=EMERGENCY_TYPE_CLIFF_DETECTED,
        severity=0.9,
        description="pitch 17°",
        timestamp_ms=1_700_000_000_000,
    ).validate()


@pytest.mark.parametrize("sev", [-0.01, 1.01, 2.0])
def test_emergency_alert_rejects_out_of_range_severity(sev):
    with pytest.raises(ValueError):
        EmergencyAlert(
            type=EMERGENCY_TYPE_CLIFF_DETECTED, severity=sev).validate()


def test_emergency_alert_rejects_unknown_type():
    with pytest.raises(ValueError):
        EmergencyAlert(type=99, severity=0.5).validate()


# ─── quaternion → pitch ────────────────────────────────────────────────

def _quat_from_pitch_deg(pitch_deg: float) -> np.ndarray:
    """Build a quaternion with pure-pitch rotation, no roll/yaw. ROS
    convention: [x, y, z, w]; pitch is rotation about y."""
    half = math.radians(pitch_deg) / 2.0
    return np.array([0.0, math.sin(half), 0.0, math.cos(half)],
                    dtype=np.float32)


def test_pitch_zero_when_identity_quat():
    q = np.array([0, 0, 0, 1], dtype=np.float32)
    assert quaternion_to_pitch_deg(q) == pytest.approx(0.0, abs=1e-3)


@pytest.mark.parametrize("deg", [10.0, 20.0, -15.0, 45.0, -60.0])
def test_pitch_round_trips_through_pure_rotation(deg):
    q = _quat_from_pitch_deg(deg)
    assert quaternion_to_pitch_deg(q) == pytest.approx(deg, abs=1e-3)


def test_pitch_clamps_at_plus_minus_90():
    # Non-unit quat with sinp out of [-1, 1] must not raise.
    big = np.array([0.0, 5.0, 0.0, 1.0], dtype=np.float32)
    assert quaternion_to_pitch_deg(big) == 90.0
    neg = np.array([0.0, -5.0, 0.0, 1.0], dtype=np.float32)
    assert quaternion_to_pitch_deg(neg) == -90.0


# ─── CliffDetector construction ────────────────────────────────────────

def test_constructor_rejects_zero_threshold():
    with pytest.raises(ValueError):
        CliffDetector(pitch_threshold_deg=0)


def test_constructor_rejects_negative_threshold():
    with pytest.raises(ValueError):
        CliffDetector(pitch_threshold_deg=-1)


def test_constructor_rejects_zero_consecutive_samples():
    with pytest.raises(ValueError):
        CliffDetector(consecutive_samples=0)


def test_constructor_rejects_negative_dedup_window():
    with pytest.raises(ValueError):
        CliffDetector(dedup_window_s=-1)


def test_default_threshold_15deg():
    d = CliffDetector()
    assert d.threshold_deg == DEFAULT_PITCH_THRESHOLD_DEG == 15.0


# ─── trigger behaviour ─────────────────────────────────────────────────

def _imu(pitch_deg: float) -> ImuData:
    return ImuData(
        header=Header(),
        orientation=_quat_from_pitch_deg(pitch_deg),
    )


def test_below_threshold_returns_none():
    d = CliffDetector()
    assert d.on_imu(_imu(5.0), now=0.0) is None
    assert d.on_imu(_imu(-10.0), now=0.0) is None


def test_single_above_threshold_sample_is_filtered():
    # Default consecutive_samples=2; a single high sample doesn't fire.
    d = CliffDetector(consecutive_samples=2)
    assert d.on_imu(_imu(20.0), now=0.0) is None
    assert d.stats["alerts"] == 0


def test_two_consecutive_samples_fire_alert():
    d = CliffDetector(consecutive_samples=2)
    assert d.on_imu(_imu(20.0), now=0.0) is None
    alert = d.on_imu(_imu(20.0), now=0.01)
    assert isinstance(alert, EmergencyAlert)
    assert alert.type == EMERGENCY_TYPE_CLIFF_DETECTED
    assert alert.severity == CLIFF_SEVERITY


def test_negative_pitch_fires_too():
    # Reverse-direction tilt over a cliff is just as bad.
    d = CliffDetector(consecutive_samples=2)
    d.on_imu(_imu(-20.0), now=0.0)
    alert = d.on_imu(_imu(-20.0), now=0.01)
    assert alert is not None
    assert "-20" in alert.description or "−20" in alert.description \
        or "-20.0" in alert.description


def test_below_threshold_resets_consecutive_count():
    # A safe sample between two above-threshold ones must reset the
    # counter — otherwise we'd alert on any 2 above-threshold samples
    # in history, not 2 *consecutive* samples.
    d = CliffDetector(consecutive_samples=3)
    d.on_imu(_imu(20.0), now=0.0)
    d.on_imu(_imu(20.0), now=0.01)
    d.on_imu(_imu(0.0),  now=0.02)        # reset
    d.on_imu(_imu(20.0), now=0.03)
    alert = d.on_imu(_imu(20.0), now=0.04)
    assert alert is None  # only 2 consecutive after the reset
    assert d.on_imu(_imu(20.0), now=0.05) is not None


# ─── dedup behaviour ───────────────────────────────────────────────────

def test_dedup_suppresses_repeated_alerts():
    d = CliffDetector(consecutive_samples=2, dedup_window_s=5.0)
    d.on_imu(_imu(20.0), now=0.0)
    first = d.on_imu(_imu(20.0), now=0.01)
    assert first is not None
    # Same sustained tilt 1 s later — within the 5 s dedup window. The
    # consecutive counter is already >= threshold so this call hits the
    # dedup gate directly (no extra below-threshold reset needed).
    suppressed = d.on_imu(_imu(20.0), now=1.00)
    assert suppressed is None
    assert d.stats["deduped"] == 1


def test_dedup_releases_after_window():
    d = CliffDetector(consecutive_samples=2, dedup_window_s=5.0)
    d.on_imu(_imu(20.0), now=0.0)
    assert d.on_imu(_imu(20.0), now=0.01) is not None
    # 6 s later — past the 5 s window; the next high-tilt sample fires
    # again. (Consecutive count is already above the threshold; we
    # don't need a fresh pair to re-arm.)
    second = d.on_imu(_imu(20.0), now=6.0)
    assert second is not None
    assert d.stats["alerts"] == 2


def test_stats_count_all_samples():
    d = CliffDetector(consecutive_samples=2)
    for _ in range(10):
        d.on_imu(_imu(5.0), now=0.0)
    assert d.stats["samples"] == 10
    assert d.stats["alerts"] == 0


def test_alert_message_validates():
    d = CliffDetector(consecutive_samples=2)
    d.on_imu(_imu(20.0), now=0.0)
    alert = d.on_imu(_imu(20.0), now=0.01)
    alert.validate()
