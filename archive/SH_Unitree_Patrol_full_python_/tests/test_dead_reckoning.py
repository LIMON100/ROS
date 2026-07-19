"""
Tests for DeadReckoner — IMU integration during RTK outage.

Verifies:
  • Stationary IMU with no rotation → no drift (gravity is canceled)
  • Constant linear acceleration → expected position via ½ a t²
  • Rotation: ω about z for π/2 → forward axis rotates 90°
  • reset() snaps state to a known truth
  • Time gap > max_dt_s → velocity zeroed (no extrapolation through hole)
  • ZUPT learns biases from a held-still period
"""
from __future__ import annotations

import math

import numpy as np
import pytest

from localization.dead_reckoning import (
    DeadReckoner,
    DrConfig,
    _omega_to_dq,
    _quat_mul,
    _quat_to_R,
)


# ════════════════════════════════════════════════════════════
# A. Quaternion math sanity
# ════════════════════════════════════════════════════════════
def test_identity_quat_rotation_is_identity_matrix():
    R = _quat_to_R(np.array([0, 0, 0, 1.0]))
    assert np.allclose(R, np.eye(3), atol=1e-9)


def test_quat_mul_with_identity_is_no_op():
    q = np.array([0.1, 0.2, 0.3, np.sqrt(1 - 0.14)])
    out = _quat_mul(q, np.array([0, 0, 0, 1.0]))
    assert np.allclose(out, q)


def test_omega_z_pi_over_dt_yields_yaw_rotation():
    # Integrate ω_z = π over dt=1 → 180° rotation about z
    dq = _omega_to_dq(np.array([0, 0, math.pi]), 1.0)
    # 180° rotation about z = (0, 0, 1, ~0)
    assert abs(dq[0]) < 1e-6
    assert abs(dq[1]) < 1e-6
    assert dq[2] == pytest.approx(1.0, abs=1e-6)
    assert abs(dq[3]) < 1e-6


# ════════════════════════════════════════════════════════════
# B. Stationary integration — should not drift
# ════════════════════════════════════════════════════════════
def test_stationary_imu_does_not_drift():
    """200 Hz stationary samples for 5 s should keep position near 0.

    The accelerometer reads +g (specific force when sitting on table);
    after gravity-comp the world-frame accel should be ~0.
    """
    dr = DeadReckoner()
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    accel_stationary = np.array([0, 0, 9.81])     # body frame, identity orientation
    gyro = np.zeros(3)
    dt = 1.0 / 200.0
    t = 0.0
    for _ in range(200 * 5):                       # 5 seconds
        t += dt
        dr.step(t, accel_stationary, gyro)
    # Should still be near origin (small numerical drift is OK)
    assert np.linalg.norm(dr.pos) < 0.05
    assert np.linalg.norm(dr.vel) < 0.05


def test_zupt_learns_accel_bias_when_still():
    """Persistent +0.1 m/s² bias on x should be partially absorbed by ZUPT."""
    dr = DeadReckoner(cfg=DrConfig(zupt_window_s=0.2, bias_alpha=0.5))
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    # Stationary but with a fixed accel bias on x
    biased_accel = np.array([0.1, 0.0, 9.81])
    gyro = np.zeros(3)
    dt = 1.0 / 100.0
    t = 0.0
    for _ in range(200):                           # 2 seconds
        t += dt
        dr.step(t, biased_accel, gyro)
    # Bias estimate should have moved toward +0.1 on x
    assert dr.accel_bias[0] > 0.05


# ════════════════════════════════════════════════════════════
# C. Constant acceleration → kinematic prediction
# ════════════════════════════════════════════════════════════
def test_constant_x_acceleration_matches_half_a_t_squared():
    """Apply +1 m/s² in body x for 2 s; expect x ≈ 2 m."""
    dr = DeadReckoner(cfg=DrConfig(zupt_window_s=999.0))   # disable ZUPT for this test
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    # Body frame x accel of 1 m/s², plus gravity in z
    accel = np.array([1.0, 0.0, 9.81])
    gyro = np.zeros(3)
    dt = 1.0 / 200.0
    t = 0.0
    for _ in range(200 * 2):
        t += dt
        dr.step(t, accel, gyro)
    # x = ½ · 1 · 2² = 2 m
    assert dr.pos[0] == pytest.approx(2.0, abs=0.05)
    assert abs(dr.pos[1]) < 0.05
    assert abs(dr.pos[2]) < 0.05
    # Velocity ≈ a·t = 2 m/s
    assert dr.vel[0] == pytest.approx(2.0, abs=0.05)


# ════════════════════════════════════════════════════════════
# D. reset() behavior
# ════════════════════════════════════════════════════════════
def test_reset_snaps_position_and_clears_history():
    dr = DeadReckoner(cfg=DrConfig(zupt_window_s=999.0))
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    # Drive forward 1 m/s² for 1 s
    dt = 1.0 / 100.0
    t = 0.0
    for _ in range(100):
        t += dt
        dr.step(t, np.array([1.0, 0, 9.81]), np.zeros(3))
    assert dr.pos[0] > 0.4

    # Reset to a new truth — position snaps, velocity/orientation cleared
    dr.reset(np.array([100.0, 50.0, 0.0]), np.array([0, 0, 0, 1.0]))
    assert dr.pos[0] == pytest.approx(100.0)
    assert dr.pos[1] == pytest.approx(50.0)
    assert np.linalg.norm(dr.vel) < 1e-6
    assert dr.distance_since_reset == 0.0


def test_distance_since_reset_grows_with_motion():
    dr = DeadReckoner(cfg=DrConfig(zupt_window_s=999.0))
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    accel = np.array([1.0, 0, 9.81])
    dt = 0.01
    t = 0.0
    for _ in range(100):
        t += dt
        dr.step(t, accel, np.zeros(3))
    # ½·1·1² = 0.5 m forward → distance_since_reset ≈ 0.5
    assert dr.distance_since_reset == pytest.approx(0.5, abs=0.05)


# ════════════════════════════════════════════════════════════
# E. Edge cases
# ════════════════════════════════════════════════════════════
def test_long_time_gap_zeros_velocity_to_avoid_extrapolation():
    """If dt > max_dt_s the integrator should not extrapolate through."""
    dr = DeadReckoner(cfg=DrConfig(max_dt_s=0.05, zupt_window_s=999.0))
    dr.reset(np.zeros(3), np.array([0, 0, 0, 1.0]))
    # Build up some velocity normally
    dt = 0.01
    t = 0.0
    for _ in range(50):
        t += dt
        dr.step(t, np.array([1.0, 0, 9.81]), np.zeros(3))
    assert dr.vel[0] > 0.4
    # Then a 1 s time gap arrives (next IMU sample is way late)
    t += 1.0
    dr.step(t, np.array([1.0, 0, 9.81]), np.zeros(3))
    # Velocity must NOT have extrapolated through the gap
    assert np.linalg.norm(dr.vel) < 1e-6


def test_first_step_does_not_extrapolate_anything():
    """The very first step() with no last_t should not change pose."""
    dr = DeadReckoner()
    dr.reset(np.array([5.0, 0, 0]), np.array([0, 0, 0, 1.0]))
    dr.step(t=10.0, accel_body=np.array([10.0, 5.0, 9.81]),
            gyro=np.array([0.5, 0.0, 0.0]))
    # Position unchanged because dt is undefined for the first sample
    assert dr.pos[0] == pytest.approx(5.0)
    assert abs(dr.pos[1]) < 1e-6
