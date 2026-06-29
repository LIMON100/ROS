# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Mission BT Fallback root per SDD-SWARM §6.1.

PDR-4 — 5 priority subtrees stacked under a Selector (BT Fallback):

  Root: Selector (Fallback semantics)
  ├── EmergencyHandler   (P0)  — Sequence(emergency? → stop → wait_release)
  ├── ManualOverride     (P1)  — Sequence(in_manual? → forward_manual_cmd)
  ├── DegradedHealth     (P2)  — Sequence(health_crit? → RTH_or_stand)
  ├── BatteryCritical    (P3)  — Sequence(battery_low? → stand_or_takeover)
  └── NormalMissionFlow  (def) — Sequence(precheck → resolve_role →
                                          role_action → tier_check →
                                          publish_progress)

Each subtree behaves as follows under the Selector (BT Fallback):
  - If its Condition is FALSE → returns FAILURE → Selector tries next
  - If its Condition is TRUE  → its Action(s) run → returns
                                SUCCESS/RUNNING → Selector short-circuits

This gives the SDD-prescribed priority: higher-numbered subtrees only
run when ALL lower-priority Conditions are false.

Pure Python — no rclpy. Tied to san_mission's BT primitives. The
rclpy node (mission_node) populates the MissionContext from
subscriptions, then ticks the tree at 10 Hz.

권원:
  * SDD-SWARM v1.5 §6.1 (Mission BT 전체 구조)
  * IDS-CMD §3.6 ManualOverrideCommand
  * IDS-CMD §3.7 EmergencyStop
  * DCN-2026-001 D-005 (Hub 승계 트리거)
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Optional, Tuple

from .behavior_tree import (
    Action, Condition, Selector, Sequence, Status, Node,
)
from .mission_context import MissionContext


# ──────────────────────────────────────────────────────────────────────
# Extended state container — adds priority-related flags to MissionContext.
# ──────────────────────────────────────────────────────────────────────

@dataclass
class PriorityState:
    """State driving the 4 priority subtrees (P0-P3).

    Populated by mission_node's subscription callbacks before each
    tick. Kept as a separate dataclass so the existing MissionContext
    isn't perturbed for Phase 2-E consumers.
    """
    # P0 EmergencyHandler — from EmergencyStop msg / button
    emergency_active:        bool = False
    emergency_release_armed: bool = False   # operator armed release?

    # P1 ManualOverride — from ManualOverrideCommand msg
    manual_mode_active: bool                          = False
    manual_cmd_vel:     Optional[Tuple[float, float]] = None  # (linear, angular)

    # P2 DegradedHealth — from SwarmHealthSummary + local self-check
    health_critical:       bool = False
    rth_destination_known: bool = True         # if not, just stand

    # P2 (also): operator deadman. mission_node sets this true when the
    # OperatorHeartbeat has been silent past the timeout (IDS §3.8). It is
    # OR'd into the DegradedHealth condition so a lost operator routes the
    # robot to RTH-or-stand — the documented failsafe.
    operator_lost:         bool = False

    # P2 RTH delegation (DCN-2026-017 + ADR-008) — Tier 2 Python BT
    # delegates Return-to-Home execution to the Tier 1 C++ action server
    # (san_rth) over /rth. mission_node populates rth_send_callable on
    # startup; this BT leaf invokes it once per health_critical episode
    # and reads rth_in_flight / rth_last_result back to decide tick
    # status. mission_bt stays pure-Python — no rclpy imports here.
    rth_send_callable:  Optional[Callable[[bool], None]] = None
    rth_in_flight:      bool          = False
    rth_last_result:    Optional[str] = None   # "OK" | "TIMEOUT" |
                                               # "GPS_LOSS_DEAD_RECKONING"
                                               # | "CANCELLED" | None

    # P3 BatteryCritical — battery_percent comes from MissionContext
    battery_critical_threshold: float = 30.0   # ≤ 30% triggers
    is_hub:                     bool = False   # for hub-takeover trigger
    hub_takeover_available:     bool = False   # deputy can take over

    # Normal flow inputs
    robot_role: str = "follower"               # "leader" | "hub" | "follower" | "deputy"

    # PreCheck status flags (populated by other subscriptions)
    ptp_synced: bool = True    # /ptp/synced
    rtk_fixed:  bool = True    # /rtk/fix_type >= RTK_FLOAT
    slam_alive: bool = True    # /slam/health
    sensors_ok: bool = True    # /sensors/health

    # Tier (filled from TierStatusChange msg) — informational
    current_tier: int = 0


@dataclass
class ExtendedMissionContext(MissionContext):
    """MissionContext + priority state. Backward-compatible: callers
    using plain MissionContext keep working."""
    priority: PriorityState = field(default_factory=PriorityState)


# ──────────────────────────────────────────────────────────────────────
# P0 — EmergencyHandler subtree
# ──────────────────────────────────────────────────────────────────────

def _emergency_active(ctx) -> bool:
    return ctx.priority.emergency_active


def _stand_and_publish_stop(ctx) -> Status:
    """Set goal = current pose, zero out velocity. Always SUCCESS."""
    if ctx.pose_xy is not None:
        ctx.goal_xy      = ctx.pose_xy
        ctx.goal_yaw_rad = ctx.yaw_rad
    # Mark state so the rclpy wrapper publishes /cmd_vel = 0
    ctx.priority.manual_cmd_vel = (0.0, 0.0)
    return Status.SUCCESS


def _wait_for_release(ctx) -> Status:
    """Block (RUNNING) until operator arms release.

    PATCH 2026-05-13 (C6): The BT no longer mutates emergency_active
    here — that field is owned by the subscription side (the
    EmergencyStop / ManualOverride callbacks). When the operator
    arms release, the subscription clears emergency_active AND
    emergency_release_armed atomically (under priority_lock). The
    BT just reads.

    Behaviour:
      * emergency_release_armed=True AND emergency_active=False
            → SUCCESS (operator finished the handshake)
      * otherwise → RUNNING
    """
    if ctx.priority.emergency_release_armed \
            and not ctx.priority.emergency_active:
        return Status.SUCCESS
    return Status.RUNNING


def build_emergency_handler() -> Node:
    """P0 priority: stand on emergency until release armed.

    PATCH 2026-05-13 (C1): memory=False — the Condition gate must be
    re-evaluated every tick so a cleared emergency drops the subtree.
    Without that, wait_for_release could loop in RUNNING after the
    operator already cleared the emergency state externally.
    """
    return Sequence(
        Condition(_emergency_active,        name="EmergencyStopRequested?"),
        Action(_stand_and_publish_stop,     name="StandAndPublishStop"),
        Action(_wait_for_release,           name="WaitForRelease"),
        name="EmergencyHandler",
        memory=False,
    )


# ──────────────────────────────────────────────────────────────────────
# P1 — ManualOverride subtree
# ──────────────────────────────────────────────────────────────────────

def _in_manual_mode(ctx) -> bool:
    return ctx.priority.manual_mode_active


def _forward_manual_cmd_vel(ctx) -> Status:
    """Action: forward operator's cmd_vel through to goal."""
    if ctx.priority.manual_cmd_vel is None:
        return Status.FAILURE
    # In a real wrapper this would publish /cmd_vel directly; here we
    # just signal the rclpy wrapper to use the manual_cmd_vel field.
    return Status.SUCCESS


def build_manual_override() -> Node:
    """P1 priority: operator's manual cmd_vel passes through.

    PATCH 2026-05-13 (C1): memory=False — same reasoning as P0.
    """
    return Sequence(
        Condition(_in_manual_mode,           name="InManualMode?"),
        Action(_forward_manual_cmd_vel,      name="ForwardManualCmdVel"),
        name="ManualOverride",
        memory=False,
    )


# ──────────────────────────────────────────────────────────────────────
# P2 — DegradedHealth subtree
# ──────────────────────────────────────────────────────────────────────

def _health_critical(ctx) -> bool:
    # P2 fires on a real health-critical state OR the operator deadman
    # (OperatorHeartbeat lost, IDS §3.8) — both route to RTH-or-stand.
    return ctx.priority.health_critical or ctx.priority.operator_lost


def _return_to_home_or_stand(ctx) -> Status:
    """DCN-2026-017 + ADR-008 — delegate RTH to san_rth C++ action server.

    Tier 2 Python BT does NOT execute navigation itself; it invokes the
    Tier 1 C++ /rth action server (combat_robot_msgs/action/ReturnToHome)
    over rclcpp_action and tracks completion via ctx flags populated by
    mission_node's goal/result callbacks.

    Tick semantics:
      * First tick with no in-flight goal and no prior result → fire
        rth_send_callable(reset_home=False) and return RUNNING.
      * Subsequent ticks while in-flight → RUNNING (no goal_xy publish;
        san_rth drives Nav2 directly).
      * Tick after result is in → SUCCESS if result=="OK", FAILURE
        otherwise (TIMEOUT / GPS_LOSS_DEAD_RECKONING / CANCELLED). The
        next health_critical transition clears these and re-arms.
      * If rth_destination_known is false OR no action callable wired
        (e.g. action server not yet up) → stand in place (fallback).
    """
    p = ctx.priority

    # Result already arrived → surface verdict
    if p.rth_last_result is not None:
        if p.rth_last_result == "OK":
            return Status.SUCCESS
        # RTH failed (TIMEOUT / GPS_LOSS_DEAD_RECKONING / CANCELLED) while
        # health is still critical. Returning FAILURE here would make the
        # DegradedHealth sequence fail and let the root Selector fall
        # through to BatteryCritical / NormalMissionFlow — i.e. a still-
        # critical robot would resume the normal mission. Per this leaf's
        # "...or stand" contract, hold safe in place and keep owning the
        # robot (RUNNING). _on_health clears rth_last_result on the
        # critical→recovered edge, which then releases P2 to lower
        # priorities.
        if ctx.pose_xy is not None:
            ctx.goal_xy = ctx.pose_xy
            ctx.goal_yaw_rad = ctx.yaw_rad
        return Status.RUNNING

    # Goal already in flight → hold (san_rth owns navigation)
    if p.rth_in_flight:
        return Status.RUNNING

    # Trigger /rth once
    if p.rth_destination_known and p.rth_send_callable is not None:
        p.rth_send_callable(False)   # reset_home_pose=False
        p.rth_in_flight = True
        return Status.RUNNING

    # Fallback: action client not wired or destination unknown → stand
    if ctx.pose_xy is not None:
        ctx.goal_xy = ctx.pose_xy
        ctx.goal_yaw_rad = ctx.yaw_rad
    return Status.RUNNING


def build_degraded_health() -> Node:
    """P2 priority: navigate home if possible, else stand.

    PATCH 2026-05-13 (C1): memory=False.
    """
    return Sequence(
        Condition(_health_critical,          name="HealthCritical?"),
        Action(_return_to_home_or_stand,     name="ReturnToHomeOrStand"),
        name="DegradedHealth",
        memory=False,
    )


# ──────────────────────────────────────────────────────────────────────
# P3 — BatteryCritical subtree (DCN-2026-001 D-005 Hub 승계 트리거)
# ──────────────────────────────────────────────────────────────────────

def _battery_low_emergency(ctx) -> bool:
    return ctx.battery_percent <= ctx.priority.battery_critical_threshold


def _immediate_stand_or_hub_takeover(ctx) -> Status:
    """For Hub at low battery: trigger hub-takeover signal (handled by
    san_role_management). Followers just stand."""
    if ctx.priority.is_hub and ctx.priority.hub_takeover_available:
        # Signal — real implementation triggers HubRoleAnnouncement
        # publication via the rclpy wrapper. Here we set a flag.
        ctx.goal_xy = ctx.pose_xy if ctx.pose_xy else (0.0, 0.0)
        return Status.SUCCESS
    # Default: immediate stand
    if ctx.pose_xy is not None:
        ctx.goal_xy = ctx.pose_xy
        ctx.goal_yaw_rad = ctx.yaw_rad
    return Status.RUNNING


def build_battery_critical() -> Node:
    """P3 priority: ≤ 30% → stand (follower) or hub takeover (hub).

    PATCH 2026-05-13 (C1): memory=False.
    """
    return Sequence(
        Condition(_battery_low_emergency,    name="BatteryLowEmergency?"),
        Action(_immediate_stand_or_hub_takeover,
                name="ImmediateStandOrHubTakeover"),
        name="BatteryCritical",
        memory=False,
    )


# ──────────────────────────────────────────────────────────────────────
# Default — NormalMissionFlow
# ──────────────────────────────────────────────────────────────────────

def _precheck_all_ok(ctx) -> bool:
    p = ctx.priority
    return p.ptp_synced and p.rtk_fixed and p.slam_alive and p.sensors_ok


def _is_leader_role(ctx) -> bool:
    return ctx.priority.robot_role == "leader"


def _is_hub_role(ctx) -> bool:
    return ctx.priority.robot_role == "hub"


def _is_follower_role(ctx) -> bool:
    return ctx.priority.robot_role == "follower"


# ─── Leader actions (stubs — real work in dedicated nodes) ─────────────
# These return SUCCESS to indicate "operation in progress / delegated".
# The actual ROS publishes happen elsewhere (formation_node, surveillance_node,
# slam_node, etc.); the BT just gates whether the leader role is active.

def _plan_global_path(ctx) -> Status:
    """Stub — delegated to Nav2 global planner."""
    return Status.SUCCESS


def _publish_breadcrumb(ctx) -> Status:
    """Stub — delegated to san_formation::formation_node leader code."""
    return Status.SUCCESS


def _publish_1s_prediction(ctx) -> Status:
    """Stub — delegated to san_formation::formation_node 10 Hz tick."""
    return Status.SUCCESS


def _manage_formation(ctx) -> Status:
    """Stub — delegated to san_formation::formation_node Hungarian."""
    return Status.SUCCESS


def _dispatch_surveillance_sectors(ctx) -> Status:
    """Stub — delegated to san_surveillance::surveillance_node 10s tick."""
    return Status.SUCCESS


def build_leader_action() -> Node:
    """Leader sub-flow per SDD §6.1."""
    return Sequence(
        Condition(_is_leader_role,           name="IsLeader?"),
        Action(_plan_global_path,            name="PlanGlobalPath"),
        Action(_publish_breadcrumb,          name="PublishBreadcrumb"),
        Action(_publish_1s_prediction,       name="Publish1sPrediction"),
        Action(_manage_formation,            name="ManageFormation"),
        Action(_dispatch_surveillance_sectors,
                name="DispatchSurveillanceSectors"),
        name="LeaderAction",
    )


# ─── Hub actions ────────────────────────────────────────────────────────

def _aggregate_slam(ctx) -> Status:
    """Stub — delegated to san_hub_slam 30-60s aggregation."""
    return Status.SUCCESS


def _broadcast_global_map(ctx) -> Status:
    """Stub — delegated to san_hub_slam global map publisher."""
    return Status.SUCCESS


def _forward_video_stream(ctx) -> Status:
    """Stub — delegated to san_video_sender SBC #2 forwarding."""
    return Status.SUCCESS


def _survey_rear_180(ctx) -> Status:
    """Stub — delegated to san_surveillance pan-tilt sweep."""
    return Status.SUCCESS


def build_hub_action() -> Node:
    """Hub sub-flow per SDD §6.1."""
    return Sequence(
        Condition(_is_hub_role,              name="IsHub?"),
        Action(_aggregate_slam,              name="AggregateSLAM"),
        Action(_broadcast_global_map,        name="BroadcastGlobalMap"),
        Action(_forward_video_stream,        name="ForwardVideoStream"),
        Action(_survey_rear_180,             name="SurveyRear180"),
        name="HubAction",
    )


# ─── Follower actions ──────────────────────────────────────────────────

def _track_leader_predicted_pose(ctx) -> Status:
    """Stub — delegated to san_follower_tier T0 PREDICTIVE_TRACK."""
    return Status.SUCCESS


def _compute_offset_by_formation(ctx) -> Status:
    """Stub — delegated to san_formation slot assignment subscriber."""
    return Status.SUCCESS


def _apply_tier_aware_motion_limit(ctx) -> Status:
    """Stub — uses ctx.priority.current_tier to scale max velocity.

    T0/T1: max_speed_recon (default 1.3 m/s)
    T2:    1.2 × max (catchup)
    T3:    max (hard catchup)
    T4:    slow (breadcrumb recovery)
    T1.5:  delegated to san_reroute_planner /cmd_vel override
    """
    return Status.SUCCESS


def _execute_surveillance_sector(ctx) -> Status:
    """Stub — delegated to follower-side SurveillanceSectorAssignment sub."""
    return Status.SUCCESS


def build_follower_action() -> Node:
    """Follower sub-flow per SDD §6.1."""
    return Sequence(
        Condition(_is_follower_role,         name="IsFollower?"),
        Action(_track_leader_predicted_pose, name="TrackLeaderPredictedPose"),
        Action(_compute_offset_by_formation, name="ComputeOffsetByFormation"),
        Action(_apply_tier_aware_motion_limit,
                name="ApplyTierAwareMotionLimit"),
        Action(_execute_surveillance_sector, name="ExecuteSurveillanceSector"),
        name="FollowerAction",
    )


# ─── Tier check + Publish progress ─────────────────────────────────────

def _check_tier(ctx) -> Status:
    """Stub — informational. Real transitions handled by tier_node.
    Updates ctx for downstream consumers. Always SUCCESS."""
    # Production: subscribe to TierStatusChange and populate
    # ctx.priority.current_tier here.
    return Status.SUCCESS


def _publish_progress(ctx) -> Status:
    """Stub — emits MissionStateCommand / progress telemetry.

    NOTE(review): tick_count is also incremented by mission_node._on_tick
    every tick, so in production it advances by 2 on ticks where
    NormalMissionFlow reaches this leaf (and by 1 otherwise) — non-
    monotonic, which can skew the _on_tick `% tick_hz` broadcast cadence.
    Left as-is intentionally: unit tests (test_mb14) treat this leaf as the
    tick_count owner, so reconciling the two owners is a design decision
    for the maintainer rather than a drive-by change.
    """
    ctx.tick_count += 1
    return Status.SUCCESS


# ─── NormalMissionFlow assembly ────────────────────────────────────────

def build_normal_mission_flow() -> Node:
    """Default subtree per SDD §6.1 — executes when all P0-P3 are clear."""
    role_action = Selector(
        build_leader_action(),
        build_hub_action(),
        build_follower_action(),
        name="ResolveAndExecuteRole",
    )
    return Sequence(
        Condition(_precheck_all_ok,          name="PreCheck"),
        role_action,
        Action(_check_tier,                  name="CheckTier"),
        Action(_publish_progress,            name="PublishProgress"),
        name="NormalMissionFlow",
    )


# ──────────────────────────────────────────────────────────────────────
# Top-level Fallback root
# ──────────────────────────────────────────────────────────────────────

def build_mission_tree() -> Node:
    """Build the full SDD §6.1 Fallback root tree.

    Returns the root Selector node. Tick it at 10 Hz from
    mission_node with an ExtendedMissionContext populated from
    subscriptions.
    """
    return Selector(
        build_emergency_handler(),
        build_manual_override(),
        build_degraded_health(),
        build_battery_critical(),
        build_normal_mission_flow(),
        name="MissionRoot",
    )
