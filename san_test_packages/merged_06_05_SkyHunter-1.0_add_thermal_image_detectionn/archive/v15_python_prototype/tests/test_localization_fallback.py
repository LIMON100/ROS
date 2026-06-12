"""
Tests for LocalizationProcess 3-tier fallback logic.

Strategy: instantiate the process *without* starting it (no fork), then
poke shared state and call _select_source() directly. This verifies the
core decision logic without setting up real multiprocessing or queues.
"""
from __future__ import annotations

import threading
import time

import numpy as np
import pytest

from core.messages import (
    RTK_FIX_DGPS,
    RTK_FIX_FIXED,
    RTK_FIX_FLOAT,
    RTK_FIX_NONE,
    Header,
    Pose6D,
    RtkFix,
)
from localization.dead_reckoning import DeadReckoner, DrConfig
from localization.localization_process import LocalizationProcess, _LocConfig


# ────────── Test fixture: bare LocalizationProcess ──────────
def make_loc(rtk_max_age_s=1.5, rtk_fixed_sigma_max=0.10, odom_drift=0.02):
    """Build a LocalizationProcess instance with state set up but NOT spawned.

    We bypass __init__ to avoid creating real mp.Queues / Config objects.
    """
    loc = LocalizationProcess.__new__(LocalizationProcess)
    loc._lcfg = _LocConfig(
        rtk_max_age_s=rtk_max_age_s,
        rtk_fixed_sigma_max=rtk_fixed_sigma_max,
        odom_drift_per_s=odom_drift,
        odom_drift_per_m=0.01,
    )
    loc._lock = threading.Lock()
    loc._latest_rtk = None
    loc._latest_slam = None
    loc._latest_imu = None
    loc._fused = {}
    loc._last_rtk_good_t = 0.0
    loc._x = np.zeros(3, dtype=np.float32)
    loc._q = np.array([0, 0, 0, 1], dtype=np.float32)
    loc._sigma_xy = 99.0
    # Dead-reckoner integration (added Phase 2)
    loc._dr = DeadReckoner(cfg=DrConfig())
    loc._dr_active = False
    return loc


def make_rtk(quality, sigma_xy=0.02, age_s=0.0, enu=(1.0, 2.0, 0.5),
             at_time=None):
    """Build a RtkFix with controllable timestamp."""
    now = at_time if at_time is not None else time.monotonic()
    h = Header(stamp=now - age_s, seq=0, frame_id="map")
    return RtkFix(
        header=h,
        lat=37.5, lon=127.0, alt=50.0,
        enu=np.array(enu, dtype=np.float32),
        fix_quality=quality, n_satellites=18,
        hdop=0.6, sigma_xy=sigma_xy, sigma_z=sigma_xy * 1.5,
    )


def make_slam_pose(x=10.0, y=20.0, z=0.0):
    return Pose6D(
        header=Header.now(frame_id="map"),
        position=np.array([x, y, z], dtype=np.float32),
        orientation=np.array([0, 0, 0, 1], dtype=np.float32),
    )


# ════════════════════════════════════════════════════════════
# Tier-1: RTK Fixed
# ════════════════════════════════════════════════════════════
def test_T1_uses_rtk_fixed_when_fresh_and_precise():
    loc = make_loc()
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, sigma_xy=0.02, at_time=now)
    src, reason, sigma = loc._select_source(now)
    assert src == "rtk_fixed"
    assert reason == ""
    assert sigma == pytest.approx(0.02)
    # And the position was copied from RTK
    assert loc._x[0] == pytest.approx(1.0)
    assert loc._x[1] == pytest.approx(2.0)


def test_T1_rejects_rtk_fixed_above_sigma_threshold():
    """Even if quality says FIXED, σ > 10 cm means we don't trust it."""
    loc = make_loc(rtk_fixed_sigma_max=0.10)
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, sigma_xy=0.50, at_time=now)
    loc._latest_slam = make_slam_pose()
    src, reason, _ = loc._select_source(now)
    assert src != "rtk_fixed"
    assert "sigma_too_high" in reason


# ════════════════════════════════════════════════════════════
# Tier-2: RTK Float
# ════════════════════════════════════════════════════════════
def test_T2_uses_rtk_float_when_fixed_unavailable():
    loc = make_loc()
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_FLOAT, sigma_xy=0.30, at_time=now)
    src, reason, sigma = loc._select_source(now)
    assert src == "rtk_float"
    assert reason == ""
    assert sigma == pytest.approx(0.30)


# ════════════════════════════════════════════════════════════
# Tier-3: Odometry fallback
# ════════════════════════════════════════════════════════════
def test_T3_falls_back_to_odometry_when_rtk_stale():
    loc = make_loc(rtk_max_age_s=1.5)
    now = time.monotonic()
    # 5 s old — way past the 1.5 s threshold
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, age_s=5.0, at_time=now)
    loc._latest_slam = make_slam_pose(x=10.0, y=20.0)
    # Pretend we had a good RTK at t = now - 5 (so drift ≈ 5 × 0.02 = 0.10 m)
    loc._last_rtk_good_t = now - 5.0
    src, reason, sigma = loc._select_source(now)
    assert src == "odometry"
    assert "stale" in reason
    # Pose comes from dead-reckoner now (used to be raw SLAM pose).
    # DR is unseeded here so it returns origin — what matters is that we
    # did NOT use the RTK ENU values (which are 1.0, 2.0 from make_rtk).
    assert loc._x[0] != pytest.approx(1.0)
    assert loc._x[1] != pytest.approx(2.0)
    # σ growth: 0.30 base + drift(5.0 s × 0.02) ≈ 0.40
    assert 0.35 < sigma < 0.50


def test_T3_falls_back_when_rtk_quality_is_no_fix():
    loc = make_loc()
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_NONE, at_time=now)
    loc._latest_slam = make_slam_pose()
    src, reason, _ = loc._select_source(now)
    assert src == "odometry"
    assert "no_fix" in reason


def test_T3_falls_back_when_rtk_quality_only_dgps():
    """DGPS (~1m) is below our acceptance threshold — only Fixed/Float."""
    loc = make_loc()
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_DGPS, at_time=now)
    loc._latest_slam = make_slam_pose()
    src, reason, _ = loc._select_source(now)
    assert src == "odometry"


def test_T3_dead_reckoning_when_no_slam_either():
    """If SLAM has never produced a pose, status = dead_reckoning."""
    loc = make_loc()
    now = time.monotonic()
    src, reason, sigma = loc._select_source(now)
    assert src == "dead_reckoning"
    assert "never_received" in reason
    assert sigma == 99.0


# ════════════════════════════════════════════════════════════
# Recovery — RTK comes back after a fall-back
# ════════════════════════════════════════════════════════════
def test_recovery_returns_to_rtk_immediately_when_back():
    """RTK lost → fallback → RTK reacquired → back to rtk_fixed in 1 cycle."""
    loc = make_loc()
    now = time.monotonic()

    # 1) Initial bad state
    loc._latest_rtk = make_rtk(RTK_FIX_NONE, at_time=now)
    loc._latest_slam = make_slam_pose()
    src, _, _ = loc._select_source(now)
    assert src == "odometry"

    # 2) RTK reacquires
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, sigma_xy=0.02,
                               age_s=0.0, at_time=now)
    src, _, _ = loc._select_source(now)
    assert src == "rtk_fixed"


def test_drift_accumulates_during_extended_outage():
    """σ should grow with time-since-RTK during odometry-only mode.

    The exact slope depends on which fallback branch fires (dead-reckoner
    vs SLAM verbatim), which depends on whether the DR has been seeded.
    Both branches should monotonically grow σ with outage duration —
    that's what we assert here, not the exact slope.
    """
    loc = make_loc(odom_drift=0.05)        # 5 cm/s drift assumption
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_NONE, at_time=now)
    loc._latest_slam = make_slam_pose()

    # Short outage
    loc._last_rtk_good_t = now - 1.0
    _, _, sigma_short = loc._select_source(now)

    # Long outage
    loc._last_rtk_good_t = now - 30.0
    _, _, sigma_long = loc._select_source(now)

    # Monotonic growth — the universal contract
    assert sigma_long > sigma_short
    # And by a meaningful amount: 29 s of additional outage at 5 cm/s drift
    # should produce at least several decimeters of σ growth, regardless
    # of which fallback branch fires.
    assert (sigma_long - sigma_short) > 0.50


# ════════════════════════════════════════════════════════════
# Boundary — exactly at age threshold
# ════════════════════════════════════════════════════════════
def test_boundary_rtk_at_exactly_max_age_is_accepted():
    """At exactly rtk_max_age_s, the fix is still considered fresh."""
    loc = make_loc(rtk_max_age_s=1.5)
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, sigma_xy=0.02,
                               age_s=1.5, at_time=now)
    src, _, _ = loc._select_source(now)
    assert src == "rtk_fixed"


def test_boundary_rtk_just_past_max_age_falls_back():
    loc = make_loc(rtk_max_age_s=1.5)
    now = time.monotonic()
    loc._latest_rtk = make_rtk(RTK_FIX_FIXED, sigma_xy=0.02,
                               age_s=1.51, at_time=now)
    loc._latest_slam = make_slam_pose()
    src, _, _ = loc._select_source(now)
    assert src == "odometry"
