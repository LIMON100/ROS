# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Mission BT standalone tests (pytest).

Pure Python — no rclpy. Validates SDD-SWARM §6.1 Fallback root
priority semantics: P0 EmergencyHandler > P1 ManualOverride > P2
DegradedHealth > P3 BatteryCritical > Default NormalMissionFlow.

Coverage:
  MB1   Empty context defaults → NormalMissionFlow only path active
  MB2   emergency_active=True  → P0 wins, all lower priorities skipped
  MB3   emergency_release_armed unsets emergency on next tick
  MB4   manual_mode_active=True → P1 wins (over P2/P3/default)
  MB5   health_critical=True   → P2 wins (over P3/default)
  MB6   battery_percent ≤ 30   → P3 wins (over default)
  MB7   Hub at low battery → hub-takeover signal path
  MB8   Multiple flags active → highest priority (P0) wins
  MB9   PreCheck failure blocks normal flow (returns FAILURE from
        the Sequence, which Selector treats as "try next"; with no
        further alternatives the root returns FAILURE)
  MB10  Leader role → leader action subtree executes
  MB11  Hub role    → hub action subtree executes
  MB12  Follower role → follower action subtree executes
  MB13  Default role behavior — invalid role falls through to FAILURE
  MB14  PublishProgress increments tick_count on each normal tick
  MB15  WaitForRelease holds RUNNING until armed
"""
from san_mission.behavior_tree import Status
from san_mission.mission_bt import (
    ExtendedMissionContext,
    build_mission_tree,
)


def _make_ctx(**overrides) -> ExtendedMissionContext:
    """Build a context with valid baseline state, override flags."""
    ctx = ExtendedMissionContext()
    ctx.pose_xy = (0.0, 0.0)
    ctx.battery_percent = 80.0
    # Baseline: all prechecks pass, follower role
    ctx.priority.ptp_synced = True
    ctx.priority.rtk_fixed  = True
    ctx.priority.slam_alive = True
    ctx.priority.sensors_ok = True
    ctx.priority.robot_role = "follower"
    for k, v in overrides.items():
        if hasattr(ctx.priority, k):
            setattr(ctx.priority, k, v)
        elif hasattr(ctx, k):
            setattr(ctx, k, v)
    return ctx


# ─── MB1: Baseline normal flow ─────────────────────────────────────────

def test_mb1_normal_flow_when_no_priorities_active():
    tree = build_mission_tree()
    ctx  = _make_ctx()
    s = tree.tick(ctx)
    # Normal flow returns SUCCESS (all stub actions succeed)
    assert s == Status.SUCCESS
    # Progress was published (tick counter incremented)
    assert ctx.tick_count == 1


# ─── MB2: P0 priority wins ─────────────────────────────────────────────

def test_mb2_emergency_p0_blocks_lower_priorities():
    tree = build_mission_tree()
    ctx  = _make_ctx(emergency_active=True)
    # Set other lower priorities — they should NOT trigger
    ctx.priority.manual_mode_active = True
    ctx.priority.health_critical    = True
    ctx.battery_percent             = 10.0   # would trigger P3

    s = tree.tick(ctx)
    # Emergency handler returns RUNNING (waits for release)
    assert s == Status.RUNNING
    # goal_xy was set to current pose (stand)
    assert ctx.goal_xy == ctx.pose_xy
    # tick_count NOT incremented (PublishProgress in NormalFlow didn't run)
    assert ctx.tick_count == 0


# ─── MB3: Emergency release ─────────────────────────────────────────────

def test_mb3_emergency_release_armed_clears_emergency():
    """PATCH 2026-05-13 (C6): release handshake is now driven by the
    subscription side. The BT only OBSERVES emergency_active and
    emergency_release_armed; it never mutates them. So in this unit
    test we explicitly clear emergency_active when the operator
    arms release (mirrors what _on_manual_override does atomically
    under the ctx.lock).
    """
    tree = build_mission_tree()
    ctx  = _make_ctx(emergency_active=True)
    # First tick — emergency RUNNING
    assert tree.tick(ctx) == Status.RUNNING
    # Operator arms release (subscription path now does both atomically).
    ctx.priority.emergency_active = False
    ctx.priority.emergency_release_armed = True
    # Second tick — release acknowledged by BT, P0 drops, normal flow runs.
    tree.tick(ctx)
    assert ctx.priority.emergency_active is False
    # After release, normal flow runs (SUCCESS)
    s2 = tree.tick(ctx)
    assert s2 == Status.SUCCESS


# ─── MB4: P1 priority ──────────────────────────────────────────────────

def test_mb4_manual_override_p1_blocks_lower():
    tree = build_mission_tree()
    ctx = _make_ctx(manual_mode_active=True,
                     manual_cmd_vel=(0.5, 0.1))
    # Lower priorities also set — shouldn't matter
    ctx.priority.health_critical = True
    ctx.battery_percent = 20.0

    s = tree.tick(ctx)
    assert s == Status.SUCCESS
    # Normal flow did NOT run
    assert ctx.tick_count == 0


# ─── MB5: P2 priority ──────────────────────────────────────────────────

def test_mb5_degraded_health_p2_blocks_battery_and_normal():
    tree = build_mission_tree()
    ctx  = _make_ctx(health_critical=True)
    ctx.battery_percent = 10.0   # P3 should be skipped

    s = tree.tick(ctx)
    # ReturnToHomeOrStand returns RUNNING
    assert s == Status.RUNNING
    assert ctx.tick_count == 0


def test_mb5b_operator_lost_routes_to_p2_rth():
    """IDS §3.8 deadman: operator_lost alone (health fine) must engage the
    P2 DegradedHealth (RTH-or-stand) branch over P3/normal."""
    tree = build_mission_tree()
    ctx  = _make_ctx(operator_lost=True)
    ctx.battery_percent = 10.0   # P3 must still be skipped

    s = tree.tick(ctx)
    assert s == Status.RUNNING   # RTH/stand owns the robot
    assert ctx.tick_count == 0   # normal flow not reached


def test_mb5c_operator_lost_yields_to_emergency():
    """P0 EmergencyStop still preempts the operator deadman (P2)."""
    tree = build_mission_tree()
    ctx  = _make_ctx(operator_lost=True, emergency_active=True)

    s = tree.tick(ctx)
    assert s == Status.RUNNING   # P0 emergency stop, not P2
    assert ctx.tick_count == 0


# ─── MB6: P3 priority ──────────────────────────────────────────────────

def test_mb6_battery_critical_blocks_normal():
    tree = build_mission_tree()
    ctx  = _make_ctx()
    ctx.battery_percent = 15.0    # below 30% threshold

    s = tree.tick(ctx)
    # ImmediateStand returns RUNNING (follower default path)
    assert s == Status.RUNNING
    assert ctx.tick_count == 0


# ─── MB7: Hub takeover at low battery ──────────────────────────────────

def test_mb7_hub_low_battery_signals_takeover():
    tree = build_mission_tree()
    ctx  = _make_ctx(is_hub=True,
                      hub_takeover_available=True,
                      robot_role="hub")
    ctx.battery_percent = 25.0

    s = tree.tick(ctx)
    # Hub-takeover path returns SUCCESS
    assert s == Status.SUCCESS


# ─── MB8: Multiple flags ────────────────────────────────────────────────

def test_mb8_multiple_priorities_only_highest_wins():
    tree = build_mission_tree()
    ctx  = _make_ctx(emergency_active=True,
                      manual_mode_active=True,
                      health_critical=True,
                      manual_cmd_vel=(1.0, 0.0))
    ctx.battery_percent = 10.0

    tree.tick(ctx)
    # Emergency wins — manual_cmd_vel was OVERWRITTEN to (0, 0) by stop
    assert ctx.priority.manual_cmd_vel == (0.0, 0.0)


# ─── MB9: PreCheck failure ──────────────────────────────────────────────

def test_mb9_precheck_fail_returns_failure_from_root():
    tree = build_mission_tree()
    ctx  = _make_ctx()
    ctx.priority.rtk_fixed = False     # PreCheck fails

    s = tree.tick(ctx)
    # All subtrees fail → Selector returns FAILURE
    assert s == Status.FAILURE
    assert ctx.tick_count == 0


# ─── MB10/11/12: Role branches ─────────────────────────────────────────

def test_mb10_leader_role_executes_leader_subtree():
    tree = build_mission_tree()
    ctx  = _make_ctx(robot_role="leader")
    s = tree.tick(ctx)
    assert s == Status.SUCCESS
    assert ctx.tick_count == 1


def test_mb11_hub_role_executes_hub_subtree():
    tree = build_mission_tree()
    ctx  = _make_ctx(robot_role="hub")
    # Use a fresh context with low battery to ensure P3 does NOT trigger
    ctx.battery_percent = 80.0
    s = tree.tick(ctx)
    assert s == Status.SUCCESS
    assert ctx.tick_count == 1


def test_mb12_follower_role_executes_follower_subtree():
    tree = build_mission_tree()
    ctx  = _make_ctx(robot_role="follower")
    s = tree.tick(ctx)
    assert s == Status.SUCCESS
    assert ctx.tick_count == 1


# ─── MB13: Invalid role ────────────────────────────────────────────────

def test_mb13_invalid_role_returns_failure():
    tree = build_mission_tree()
    ctx  = _make_ctx(robot_role="deputy")    # not handled in role selector
    s = tree.tick(ctx)
    # ResolveAndExecuteRole Selector: all 3 role conditions fail →
    # selector returns FAILURE → NormalMissionFlow Sequence returns
    # FAILURE → root Selector tries no more → FAILURE.
    assert s == Status.FAILURE


# ─── MB14: PublishProgress ─────────────────────────────────────────────

def test_mb14_publish_progress_increments_tick_count():
    tree = build_mission_tree()
    ctx  = _make_ctx()
    for _ in range(5):
        tree.tick(ctx)
    assert ctx.tick_count == 5


# ─── MB15: WaitForRelease blocking ─────────────────────────────────────

def test_mb15_wait_for_release_holds_running():
    tree = build_mission_tree()
    ctx  = _make_ctx(emergency_active=True)
    # 5 ticks without release armed — must stay RUNNING
    for _ in range(5):
        s = tree.tick(ctx)
        assert s == Status.RUNNING
    # Tick count never advanced
    assert ctx.tick_count == 0
