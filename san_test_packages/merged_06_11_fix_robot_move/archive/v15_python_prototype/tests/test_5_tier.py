"""
5-Tier escape FSM + Breadcrumb buffer tests (SDD Rev.A.5 §6.7).

The TierManager is pure logic — no IPC, no threading. Each test drives
update() with synthetic δ values and asserts the resulting tier.
"""
from __future__ import annotations

import pytest

from core.messages import (
    TIER_T0,
    TIER_T1,
    TIER_T1_5,
    TIER_T2,
    TIER_T3,
    TIER_T4,
)
from swarm.breadcrumb import BreadcrumbBuffer
from swarm.tier_manager import TierManager

D0 = 3.0


def _tm() -> TierManager:
    return TierManager(d0_m=D0)


# ════════════════════════════════════════════════════════════════
# test_t0_predictive_track (2 tests)
# ════════════════════════════════════════════════════════════════
def test_t0_with_fresh_target_inside_envelope():
    tm = _tm()
    upd = tm.update(delta_m=1.0, has_fresh_target=True, reroute_active=False)
    assert upd.cur == TIER_T0


def test_t0_drops_to_normal_ladder_when_target_expires():
    tm = _tm()
    tm.update(delta_m=1.0, has_fresh_target=True)
    upd = tm.update(delta_m=1.0, has_fresh_target=False)
    # δ < 1.2·d₀ → T1
    assert upd.cur == TIER_T1


# ════════════════════════════════════════════════════════════════
# test_t1_5_auto_reroute_2m_offset (3 tests)
# ════════════════════════════════════════════════════════════════
def test_t1_5_when_local_planner_reroutes_within_envelope():
    tm = _tm()
    upd = tm.update(delta_m=1.5, has_fresh_target=False, reroute_active=True)
    assert upd.cur == TIER_T1_5


def test_t1_5_does_not_apply_outside_2m_envelope():
    """A reroute that wanders past 2·d₀ is a real escape, not a nudge."""
    tm = _tm()
    upd = tm.update(delta_m=2.5 * D0, reroute_active=True)
    assert upd.cur == TIER_T4


def test_t1_5_takes_precedence_over_t2_threshold_when_active():
    """If the planner is rerouting and δ is in the T2 band but still <
    2·d₀, we want to label it T1.5 (planner is handling it), not T2."""
    tm = _tm()
    upd = tm.update(delta_m=1.3 * D0, reroute_active=True)
    assert upd.cur == TIER_T1_5


# ════════════════════════════════════════════════════════════════
# test_t1_normal_pid (2 tests)
# ════════════════════════════════════════════════════════════════
def test_t1_default_state():
    tm = _tm()
    assert tm.tier == TIER_T1


def test_t1_remains_t1_for_small_delta():
    tm = _tm()
    upd = tm.update(delta_m=0.5)
    assert upd.cur == TIER_T1
    assert upd.transitioned is False


# ════════════════════════════════════════════════════════════════
# test_t2_t3_catchup_with_hysteresis (4 tests)
# ════════════════════════════════════════════════════════════════
def test_t2_entered_at_1_2_d0():
    tm = _tm()
    upd = tm.update(delta_m=1.2 * D0)
    assert upd.cur == TIER_T2


def test_t3_entered_at_1_5_d0():
    tm = _tm()
    upd = tm.update(delta_m=1.5 * D0)
    assert upd.cur == TIER_T3


def test_t2_exit_requires_deadband_below_threshold():
    """T2 → T1 only when δ ≤ 1.2·d₀ * 0.95. At exactly 1.2·d₀ we stay in T2."""
    tm = _tm()
    tm.update(delta_m=1.2 * D0)
    assert tm.tier == TIER_T2
    upd = tm.update(delta_m=1.2 * D0 * 0.97)         # within the deadband
    assert upd.cur == TIER_T2
    upd = tm.update(delta_m=1.2 * D0 * 0.90)         # past the deadband
    assert upd.cur == TIER_T1


def test_t3_to_t2_at_1_5_d0_boundary():
    tm = _tm()
    tm.update(delta_m=1.7 * D0)     # → T3
    assert tm.tier == TIER_T3
    upd = tm.update(delta_m=1.4 * D0)
    assert upd.cur == TIER_T2


# ════════════════════════════════════════════════════════════════
# test_t4_breadcrumb_recovery (3 tests)
# ════════════════════════════════════════════════════════════════
def test_t4_entered_at_2_d0():
    tm = _tm()
    upd = tm.update(delta_m=2.0 * D0 + 0.01)
    assert upd.cur == TIER_T4


def test_t4_skips_t3_on_recovery_per_spec():
    """Per SDD §6.7 Table: T4 → T2 directly when δ falls below 2·d₀."""
    tm = _tm()
    tm.update(delta_m=2.5 * D0)     # → T4
    assert tm.tier == TIER_T4
    upd = tm.update(delta_m=1.7 * D0)
    # Per spec we go T4 → T2 (not T4 → T3)
    assert upd.cur == TIER_T2


def test_breadcrumb_buffer_samples_by_distance_and_time():
    bb = BreadcrumbBuffer(min_dist_m=1.0, min_interval_s=0.5,
                          max_points=10, max_age_s=60.0)
    assert bb.offer(x=0.0, y=0.0, yaw=0.0, stamp=0.0) is not None
    # Within 0.3 m and 0.1 s — rejected
    assert bb.offer(x=0.3, y=0.0, yaw=0.0, stamp=0.1) is None
    # Past the time threshold even though distance is small — accepted
    assert bb.offer(x=0.3, y=0.0, yaw=0.0, stamp=0.6) is not None
    # Past the distance threshold — accepted
    assert bb.offer(x=2.0, y=0.0, yaw=0.0, stamp=0.8) is not None
    assert len(bb) == 3
    assert bb.latest().x == pytest.approx(2.0)


# ════════════════════════════════════════════════════════════════
# test_tier_transitions_no_chattering (2 tests)
# ════════════════════════════════════════════════════════════════
def test_no_chattering_at_t1_t2_boundary():
    """Oscillation around δ ≈ 1.2·d₀ shouldn't produce per-tick transitions."""
    tm = _tm()
    deltas = [1.18 * D0, 1.21 * D0, 1.18 * D0, 1.21 * D0, 1.18 * D0]
    transitions = 0
    for d in deltas:
        upd = tm.update(delta_m=d)
        transitions += int(upd.transitioned)
    # First step crosses → at most one transition in/out across the 5 ticks
    assert transitions <= 1


def test_breadcrumb_buffer_evicts_old_samples():
    bb = BreadcrumbBuffer(min_dist_m=1.0, min_interval_s=0.5,
                          max_points=100, max_age_s=2.0)
    bb.offer(x=0.0, y=0.0, yaw=0.0, stamp=0.0)
    bb.offer(x=2.0, y=0.0, yaw=0.0, stamp=1.0)
    bb.offer(x=4.0, y=0.0, yaw=0.0, stamp=2.5)
    # max_age_s=2.0; points older than (2.5 - 2.0) = 0.5 s are evicted.
    bb._evict_old(now=2.5)
    stamps = [p.stamp for p in bb.snapshot()]
    assert all(s >= 0.5 for s in stamps)
