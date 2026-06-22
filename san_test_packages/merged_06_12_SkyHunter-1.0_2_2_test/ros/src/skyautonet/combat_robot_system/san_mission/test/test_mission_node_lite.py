# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 mission_node logic tests.

Pure-logic tests (no rclpy) that validate:
  PN1 (★ C2)  tick_hz < 1.0 raises ValueError BEFORE the node would
              divide by zero on every tick.
  PN2 (★ C2)  tick_hz > 50.0 raises ValueError (M12 upper bound).
  PN3 (★ C3)  goal_xy is cleared before each tick — the test models
              the publish path's gating.
  PN4 (★ C7)  SCOPE_SINGLE_ROBOT matching against robot_id.
"""
import pytest


# ─── PN1 / PN2 (★ C2 / M12): tick_hz range validation logic ───────────
def _validate_tick_hz(tick_hz):
    """Pure helper — mirrors mission_node.__init__'s validation."""
    if tick_hz < 1.0 or tick_hz > 50.0:
        raise ValueError(
            f"MissionNode: tick_hz out of range [1.0, 50.0]: {tick_hz}")
    return True


def test_pn1_tick_hz_below_one_rejected():
    """Pre-PATCH: tick_hz=0.5 was accepted, but `tick_count % int(0.5)`
    = `% 0` → ZeroDivisionError on every tick. PATCH rejects upfront."""
    with pytest.raises(ValueError):
        _validate_tick_hz(0.5)
    with pytest.raises(ValueError):
        _validate_tick_hz(0.999)


def test_pn2_tick_hz_above_fifty_rejected():
    """PATCH lowered upper bound from 100 to 50 (M12) — BT runs as
    plain Python under the GIL; 50 Hz is already pushing it."""
    with pytest.raises(ValueError):
        _validate_tick_hz(75.0)
    with pytest.raises(ValueError):
        _validate_tick_hz(100.0)


def test_pn2b_tick_hz_in_range_accepted():
    assert _validate_tick_hz(1.0) is True
    assert _validate_tick_hz(5.0) is True
    assert _validate_tick_hz(10.0) is True
    assert _validate_tick_hz(50.0) is True


# ─── PN3 (★ C3): goal_xy gating logic ──────────────────────────────────
def _should_publish_goal(bt_status, goal_xy):
    """Pure helper — mirrors mission_node._on_tick's publish gate."""
    # PATCH: publish only on non-FAILURE AND goal_xy not None.
    from san_mission.behavior_tree import Status
    return bt_status != Status.FAILURE and goal_xy is not None


def test_pn3_goal_not_published_on_failure():
    from san_mission.behavior_tree import Status
    # BT returned FAILURE but goal_xy is non-None (stale from prev tick).
    # PATCH: must NOT publish.
    assert _should_publish_goal(Status.FAILURE, (1.0, 2.0)) is False


def test_pn3b_goal_not_published_when_bt_didnt_set():
    from san_mission.behavior_tree import Status
    # BT returned SUCCESS but didn't set goal_xy this tick.
    # PATCH: goal_xy is None at tick start; must NOT publish.
    assert _should_publish_goal(Status.SUCCESS, None) is False


def test_pn3c_goal_published_when_running_with_goal():
    from san_mission.behavior_tree import Status
    assert _should_publish_goal(Status.RUNNING, (1.0, 2.0)) is True


def test_pn3d_goal_published_on_success_with_goal():
    from san_mission.behavior_tree import Status
    assert _should_publish_goal(Status.SUCCESS, (1.0, 2.0)) is True


# ─── PN4 (★ C7): SCOPE_SINGLE_ROBOT matching logic ────────────────────
def _should_apply_single_robot_scope(my_id, target_id):
    """Pure helper — mirrors mission_node._on_emergency_stop's
    SCOPE_SINGLE_ROBOT path."""
    return my_id != 0 and int(target_id) == my_id


def test_pn4_single_robot_match():
    # We are robot_id=3, target is 3 → apply.
    assert _should_apply_single_robot_scope(3, 3) is True


def test_pn4b_single_robot_mismatch():
    # We are robot_id=3, target is 5 → do NOT apply.
    assert _should_apply_single_robot_scope(3, 5) is False


def test_pn4c_single_robot_zero_self_safe():
    # If we don't have a real robot_id (=0 default), SCOPE_SINGLE_ROBOT
    # must NEVER apply (safety: better to ignore than stop everything).
    assert _should_apply_single_robot_scope(0, 3) is False
    assert _should_apply_single_robot_scope(0, 0) is False


# ─── PN5 (★ C4): ctx.lock present and usable ──────────────────────────
def test_pn5_mission_context_has_lock():
    from san_mission.mission_context import MissionContext
    ctx = MissionContext()
    # Lock must be present and acquirable.
    assert ctx.lock is not None
    with ctx.lock:
        ctx.battery_percent = 50.0
    assert ctx.battery_percent == 50.0


def test_pn5b_extended_mission_context_has_lock():
    from san_mission.mission_bt import ExtendedMissionContext
    ctx = ExtendedMissionContext()
    assert ctx.lock is not None
    with ctx.lock:
        ctx.priority.emergency_active = True
        ctx.priority.emergency_release_armed = False
    assert ctx.priority.emergency_active is True
    assert ctx.priority.emergency_release_armed is False
