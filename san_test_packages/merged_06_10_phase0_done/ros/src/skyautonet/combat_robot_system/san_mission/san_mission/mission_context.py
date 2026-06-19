# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 9-10 — Mission shared context + BT leaves.

MissionContext is the shared state container passed to every behavior
tree node's tick() method. It carries:
  * Latest pose / robot_status
  * Operational mode (via OperationalModeController)
  * Output target (goal pose to publish)
  * Leadership flag (this node currently elected leader?)

The BT itself is built from the primitives in behavior_tree.py — this
module supplies the mission-specific Action / Condition leaves.

PATCH 2026-05-13 (san_mission deep-dive C4):
  * MissionContext.lock — threading.RLock that the rclpy node uses to
    serialize subscription writes against the BT tick read path.
    The GIL makes single-attribute reads/writes atomic but multi-
    field updates (e.g. manual_mode_active + manual_cmd_vel together)
    are NOT atomic and the BT can read a torn snapshot. The lock
    closes that hole.
"""
from __future__ import annotations

import threading
from dataclasses import dataclass, field
from typing import Optional, Tuple

from .behavior_tree import Action, Condition, Status
from .operational_modes import OperationalModeController


@dataclass
class MissionContext:
    """Shared state for behavior tree leaves."""
    # Live sensor snapshots
    pose_xy: Optional[Tuple[float, float]] = None     # world frame
    yaw_rad:                       float   = 0.0
    battery_percent:               float   = 100.0
    in_limp_mode:                  bool    = False
    slam_healthy:                  bool    = True

    # Election state — true if this robot is currently the leader
    is_leader:                     bool    = False

    # Mode controller (PIN-gated)
    mode: OperationalModeController = field(
        default_factory=OperationalModeController)

    # Output: target goal pose for the locomotion layer
    goal_xy:   Optional[Tuple[float, float]] = None
    goal_yaw_rad:               Optional[float] = None

    # Stats
    tick_count: int = 0

    # R-13 (PR #115) + Phase 7: shared RLock guards all field
    # reads/writes that involve more than one related field —
    # callbacks (pose, status, priority state, manual_override) and
    # BT tick snapshots. Under MultiThreadedExecutor the two-statement
    # writes in _on_pose / _on_status would tear without it.
    lock: threading.RLock = field(default_factory=threading.RLock)


# ─── Mission-specific BT leaves ─────────────────────────────────────────

def is_leader(ctx: MissionContext) -> bool:
    return ctx.is_leader


def slam_is_healthy(ctx: MissionContext) -> bool:
    return ctx.slam_healthy


def battery_above_threshold(threshold_percent: float):
    """Condition factory — returns a function suitable for Condition."""
    def _check(ctx: MissionContext) -> bool:
        return ctx.battery_percent > threshold_percent
    return _check


def not_in_limp_mode(ctx: MissionContext) -> bool:
    return not ctx.in_limp_mode


def emit_hold_goal(ctx: MissionContext) -> Status:
    """Action: set goal to current pose (hold)."""
    if ctx.pose_xy is None:
        return Status.FAILURE
    ctx.goal_xy        = ctx.pose_xy
    ctx.goal_yaw_rad   = ctx.yaw_rad
    return Status.SUCCESS


def emit_advance_goal(distance_m: float):
    """Action factory: move `distance_m` forward from current pose.
    Pure utility — real planner replaces this in production.
    """
    import math

    def _advance(ctx: MissionContext) -> Status:
        if ctx.pose_xy is None:
            return Status.FAILURE
        x, y = ctx.pose_xy
        ctx.goal_xy = (
            x + distance_m * math.cos(ctx.yaw_rad),
            y + distance_m * math.sin(ctx.yaw_rad),
        )
        ctx.goal_yaw_rad = ctx.yaw_rad
        return Status.SUCCESS
    return _advance


# ─── Pre-built example mission tree ─────────────────────────────────────

def build_patrol_tree(min_battery_percent: float = 15.0):
    """Builds the canonical patrol behavior tree:
        Sequence(
            Condition(is_leader),
            Condition(slam_is_healthy),
            Condition(not_in_limp_mode),
            Condition(battery > min),
            Selector(
                Sequence(Condition(...), Action(emit_advance_goal)),
                Action(emit_hold_goal)
            )
        )
    Returns the root node. Real production tree is built in
    mission_node from policy files.
    """
    from .behavior_tree import Selector, Sequence
    return Sequence(
        Condition(is_leader,           name="is_leader"),
        Condition(slam_is_healthy,     name="slam_healthy"),
        Condition(not_in_limp_mode,    name="not_limp"),
        Condition(battery_above_threshold(min_battery_percent),
                  name="battery_ok"),
        Selector(
            Action(emit_advance_goal(1.0), name="advance_1m"),
            Action(emit_hold_goal,         name="hold_position"),
            name="movement_selector",
        ),
        name="patrol",
    )
