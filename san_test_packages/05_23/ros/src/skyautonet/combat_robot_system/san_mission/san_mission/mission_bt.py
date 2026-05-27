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
from typing import Optional, Tuple

from .behavior_tree import (
    Action, Condition, Selector, Sequence, Status, Node,
)
from .mission_context import MissionContext
import time
import math

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

    # --- INJECTED PRIORITY FLAGS ---
    combat_active: bool = False
    combat_target_xy: Tuple[float, float] = (0.0, 0.0)
    stuck_active: bool = False
    stuck_recovery_start: float = 0.0
    # -------------------------------


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
    """Block (RUNNING) until operator arms release."""
    if ctx.priority.emergency_release_armed:
        ctx.priority.emergency_active = False
        ctx.priority.emergency_release_armed = False
        return Status.SUCCESS
    return Status.RUNNING


def build_emergency_handler() -> Node:
    """P0 priority: stand on emergency until release armed."""
    return Sequence(
        Condition(_emergency_active,        name="EmergencyStopRequested?"),
        Action(_stand_and_publish_stop,     name="StandAndPublishStop"),
        Action(_wait_for_release,           name="WaitForRelease"),
        name="EmergencyHandler",
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
    """P1 priority: operator's manual cmd_vel passes through."""
    return Sequence(
        Condition(_in_manual_mode,           name="InManualMode?"),
        Action(_forward_manual_cmd_vel,      name="ForwardManualCmdVel"),
        name="ManualOverride",
    )


# ──────────────────────────────────────────────────────────────────────
# P2 — DegradedHealth subtree
# ──────────────────────────────────────────────────────────────────────

def _health_critical(ctx) -> bool:
    return ctx.priority.health_critical


def _return_to_home_or_stand(ctx) -> Status:
    """If RTH known, set goal to home; else hold position."""
    if ctx.priority.rth_destination_known and ctx.pose_xy is not None:
        # Stub — production would query stored home pose
        ctx.goal_xy = (0.0, 0.0)
        ctx.goal_yaw_rad = 0.0
        return Status.RUNNING       # navigation will resolve over time
    # Fall back to stand
    if ctx.pose_xy is not None:
        ctx.goal_xy = ctx.pose_xy
        ctx.goal_yaw_rad = ctx.yaw_rad
    return Status.RUNNING


def build_degraded_health() -> Node:
    """P2 priority: navigate home if possible, else stand."""
    return Sequence(
        Condition(_health_critical,          name="HealthCritical?"),
        Action(_return_to_home_or_stand,     name="ReturnToHomeOrStand"),
        name="DegradedHealth",
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
    """P3 priority: ≤ 30% → stand (follower) or hub takeover (hub)."""
    return Sequence(
        Condition(_battery_low_emergency,    name="BatteryLowEmergency?"),
        Action(_immediate_stand_or_hub_takeover,
                name="ImmediateStandOrHubTakeover"),
        name="BatteryCritical",
    )


def _is_stuck(ctx) -> bool:
    return ctx.priority.stuck_active

def _execute_recovery(ctx) -> Status:
    # Reverse + Twist for 3 seconds
    if time.time() - ctx.priority.stuck_recovery_start < 3.0:
        ctx.priority.manual_cmd_vel = (-0.8, -0.6) 
        return Status.SUCCESS
    else:
        ctx.priority.stuck_active = False # Recovered
        ctx.priority.manual_cmd_vel = None
        ctx.last_pose_time = time.time() # Reset timer
        return Status.SUCCESS

def build_stuck_recovery() -> Node:
    """P2.5 priority: Reverse out of stuck positions."""
    return Sequence(
        Condition(_is_stuck, name="IsStuck?"),
        Action(_execute_recovery, name="ExecuteReverseRecovery"),
        name="StuckRecovery",
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
    """Injected Waypoint & Combat Logic."""
    if ctx.pose_xy is None:
        return Status.FAILURE

    # 1. Combat Override (Fire Net Encircle)
    if ctx.priority.combat_active:
        tx, ty = ctx.priority.combat_target_xy
        angle_to_target = math.atan2(ctx.pose_xy[1] - ty, ctx.pose_xy[0] - tx)
        ctx.goal_xy = (tx + 5.0 * math.cos(angle_to_target), ty + 5.0 * math.sin(angle_to_target))
        ctx.goal_yaw_rad = angle_to_target + math.pi
        return Status.SUCCESS

    # 2. Mission Complete Check
    if ctx.current_wp_index >= len(ctx.waypoints):
        return Status.SUCCESS

    # 3. Halt Mode (Perimeter Sweep 20s)
    if ctx.is_halt_mode:
        if time.time() - ctx.halt_start_time > 20.0:
            ctx.is_halt_mode = False
            ctx.current_wp_index += 1
        else:
            ctx.goal_xy = ctx.pose_xy # Stand still
        return Status.SUCCESS

    # 4. Standard Waypoint Navigation
    tx, ty = ctx.waypoints[ctx.current_wp_index]
    dist = math.hypot(tx - ctx.pose_xy[0], ty - ctx.pose_xy[1])
    
    if dist < 1.0: # Reached Waypoint
        ctx.is_halt_mode = True
        ctx.halt_start_time = time.time()
        ctx.goal_xy = ctx.pose_xy
    else:
        ctx.goal_xy = (tx, ty)
        
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
    """Stub — emits MissionStateCommand / progress telemetry."""
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
        build_stuck_recovery(),
        build_normal_mission_flow(),
        name="MissionRoot",
    )
