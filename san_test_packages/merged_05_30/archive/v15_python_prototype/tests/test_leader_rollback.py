"""Tests for LeaderRollbackChecker (P1-4, SDD Rev.A.6 §7.6)."""
from __future__ import annotations

import time

from mission.leader_rollback import (
    FollowerStatus,
    LeaderRollbackChecker,
    RollbackState,
)
from swarm.tier_manager import Tier


def _status(fid: int, tier: Tier) -> FollowerStatus:
    return FollowerStatus(follower_id=fid, tier=tier)


def test_no_rollback_when_below_50_pct():
    chk = LeaderRollbackChecker()
    followers = [
        _status(1, Tier.T0),
        _status(2, Tier.T0),
        _status(3, Tier.T2),   # not struggling (T3+ only)
        _status(4, Tier.T3),
        _status(5, Tier.T1),
    ]   # 1/5 struggling = 20 %
    ev = chk.update_followers(followers)
    assert ev is None
    assert chk.state == RollbackState.NORMAL


def test_rollback_initiated_at_50_pct():
    chk = LeaderRollbackChecker()
    followers = [
        _status(1, Tier.T3),
        _status(2, Tier.T4),
        _status(3, Tier.T3),
        _status(4, Tier.T0),
        _status(5, Tier.T0),
    ]   # 3/5 = 60 %
    ev = chk.update_followers(followers)
    assert ev is not None
    assert chk.state == RollbackState.ROLLBACK_INITIATED
    assert ev.ratio >= 0.5


def test_rollback_state_machine_transitions():
    chk = LeaderRollbackChecker()
    bad = [_status(i, Tier.T3 if i < 3 else Tier.T0) for i in range(5)]
    chk.update_followers(bad)
    assert chk.state == RollbackState.ROLLBACK_INITIATED
    chk.update_followers(bad)   # advance one tick
    assert chk.state == RollbackState.RETREATING


def test_recovery_via_hysteresis():
    chk = LeaderRollbackChecker()
    bad = [_status(i, Tier.T3 if i < 3 else Tier.T0) for i in range(5)]
    chk.update_followers(bad)
    chk.update_followers(bad)
    assert chk.state == RollbackState.RETREATING
    # Followers caught up: 0 % struggling
    good = [_status(i, Tier.T0) for i in range(5)]
    ev = chk.update_followers(good)
    assert ev is not None
    assert chk.state == RollbackState.NORMAL
    assert "RECOVERY" in ev.message


def test_no_recovery_above_30_pct_hysteresis():
    chk = LeaderRollbackChecker()
    bad = [_status(i, Tier.T3 if i < 3 else Tier.T0) for i in range(5)]
    chk.update_followers(bad)
    chk.update_followers(bad)
    # 2/5 = 40 % — within hysteresis band, must NOT recover
    mid = ([_status(0, Tier.T3), _status(1, Tier.T3)]
           + [_status(i, Tier.T0) for i in range(2, 5)])
    chk.update_followers(mid)
    assert chk.state in (RollbackState.RETREATING,
                         RollbackState.ROLLBACK_INITIATED,
                         RollbackState.REPLANNING)


def test_retreat_target_from_stable_history():
    chk = LeaderRollbackChecker()
    chk.record_stable_position(1.0, 2.0, 0.0)
    time.sleep(0.01)
    chk.record_stable_position(3.0, 4.0, 0.5)
    time.sleep(0.01)
    bad = [_status(i, Tier.T3 if i < 3 else Tier.T0) for i in range(5)]
    chk.update_followers(bad)            # initiate
    target = chk.get_retreat_target()
    assert target is not None
    # Latest stable position recorded before rollback start
    assert target.x in (1.0, 3.0)


def test_no_viable_path_when_replan_fails():
    chk = LeaderRollbackChecker()
    chk.state = RollbackState.REPLANNING
    chk.replan_complete(success=False)
    assert chk.state == RollbackState.NO_VIABLE_PATH
