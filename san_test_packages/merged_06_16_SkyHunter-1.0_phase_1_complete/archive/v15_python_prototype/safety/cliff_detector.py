"""IMU-driven cliff detector (PHASE 7 §7.5).

The Robosense E1's 120° horizontal FoV leaves the rear + side-rear as
LiDAR blind spots. A robot reversing toward a cliff would never see it
in the point cloud — so we add a complementary IMU-pitch monitor that
fires an EmergencyAlert when the chassis tips beyond a safe threshold.

This module is pure compute: it takes ImuData in and returns
EmergencyAlert (or None). Hysteresis + dedup logic lives here, so a
single noisy IMU sample doesn't flap the alert, and a sustained
sharp-pitch condition emits exactly one alert (not 200 Hz worth).

Wire-up (caller's responsibility):
    detector = CliffDetector()
    on every IMU tick:
        alert = detector.on_imu(imu_msg)
        if alert is not None:
            publish(queues.emergency_alert, alert)
"""
from __future__ import annotations

import math
import time
from typing import Optional

import numpy as np

from core.messages import (
    EMERGENCY_TYPE_CLIFF_DETECTED,
    EmergencyAlert,
    ImuData,
)

# Default pitch threshold (deg) — spec line: "pitch > 15° → cliff alert".
# Below this is normal terrain; above it the chassis is likely tilting
# over an unobserved drop.
DEFAULT_PITCH_THRESHOLD_DEG = 15.0

# Severity for the cliff event — spec line: severity = 0.9.
CLIFF_SEVERITY = 0.9

# Hysteresis: the detector requires `consecutive_samples` IMU readings
# above the threshold before firing. At 100 Hz IMU + 2 samples = 20 ms
# of sustained tilt — enough to filter a single rattled sample without
# adding meaningful latency.
DEFAULT_CONSECUTIVE_SAMPLES = 2

# Dedup window: after firing, suppress further alerts for this duration
# even if the pitch stays high. The first alert woke the operator;
# repeating it 100×/s would just spam.
DEFAULT_DEDUP_WINDOW_S = 5.0


def quaternion_to_pitch_deg(quat_xyzw: np.ndarray) -> float:
    """Extract pitch (Y-axis rotation) from a quaternion in degrees.

    `quat_xyzw` is the standard ROS2 [x, y, z, w] layout (matches
    `ImuData.orientation` in core.messages). Returns pitch in
    [-90°, +90°]; the quaternion may be non-unit-norm but is
    expected to be near-unit (raw IMU output).
    """
    x, y, z, w = (float(v) for v in quat_xyzw[:4])
    sinp = 2.0 * (w * y - z * x)
    # Numerical guard against |sinp| > 1 from a slightly non-unit quat.
    if sinp >= 1.0:
        return 90.0
    if sinp <= -1.0:
        return -90.0
    return math.degrees(math.asin(sinp))


class CliffDetector:
    """Stateful pitch monitor with hysteresis + dedup.

    Threading: not internally synchronised. The caller is the IMU
    consumer thread, which is single-producer in the existing
    SafetyProcess pattern.
    """

    def __init__(
        self,
        pitch_threshold_deg: float = DEFAULT_PITCH_THRESHOLD_DEG,
        consecutive_samples: int = DEFAULT_CONSECUTIVE_SAMPLES,
        dedup_window_s: float = DEFAULT_DEDUP_WINDOW_S,
    ):
        if pitch_threshold_deg <= 0:
            raise ValueError(
                f"pitch_threshold_deg must be positive: "
                f"{pitch_threshold_deg}")
        if consecutive_samples < 1:
            raise ValueError(
                f"consecutive_samples must be >= 1: {consecutive_samples}")
        if dedup_window_s < 0:
            raise ValueError(
                f"dedup_window_s must be non-negative: {dedup_window_s}")
        self._threshold_deg = float(pitch_threshold_deg)
        self._consecutive_samples = int(consecutive_samples)
        self._dedup_window_s = float(dedup_window_s)
        self._consecutive_count = 0
        self._last_alert_t: Optional[float] = None
        self._stats = {"samples": 0, "alerts": 0, "deduped": 0}

    @property
    def threshold_deg(self) -> float:
        return self._threshold_deg

    @property
    def stats(self) -> dict:
        return dict(self._stats)

    def on_imu(
        self,
        msg: ImuData,
        now: Optional[float] = None,
    ) -> Optional[EmergencyAlert]:
        """Process one IMU sample. Returns an EmergencyAlert when the
        threshold is exceeded for `consecutive_samples` ticks AND the
        dedup window has elapsed since the last alert.

        `now` is wall-clock seconds (monotonic preferred); defaults to
        time.monotonic() — pass a fixed value in tests so the dedup
        window can be exercised without sleeping.
        """
        self._stats["samples"] += 1
        pitch_deg = quaternion_to_pitch_deg(msg.orientation)
        if abs(pitch_deg) < self._threshold_deg:
            # Below threshold — reset the hysteresis counter so a brief
            # tilt doesn't accumulate toward an alert across a flat
            # stretch.
            self._consecutive_count = 0
            return None

        self._consecutive_count += 1
        if self._consecutive_count < self._consecutive_samples:
            return None

        # Dedup: suppress repeats within the window.
        t = now if now is not None else time.monotonic()
        if (self._last_alert_t is not None
                and (t - self._last_alert_t) < self._dedup_window_s):
            self._stats["deduped"] += 1
            return None

        self._last_alert_t = t
        self._stats["alerts"] += 1
        alert = EmergencyAlert(
            type=EMERGENCY_TYPE_CLIFF_DETECTED,
            severity=CLIFF_SEVERITY,
            description=(
                f"Sharp pitch {pitch_deg:+.1f}° — possible cliff "
                f"(LiDAR blind-spot reverse?)"),
            timestamp_ms=int(t * 1000),
        )
        alert.validate()
        return alert


__all__ = (
    "CLIFF_SEVERITY",
    "CliffDetector",
    "DEFAULT_CONSECUTIVE_SAMPLES",
    "DEFAULT_DEDUP_WINDOW_S",
    "DEFAULT_PITCH_THRESHOLD_DEG",
    "quaternion_to_pitch_deg",
)
