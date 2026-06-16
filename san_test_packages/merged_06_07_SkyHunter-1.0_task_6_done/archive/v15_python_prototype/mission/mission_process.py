"""
MissionProcess: orchestrates S1 patrol via behavior tree.

Threads:
  • PoseSub          : track current robot pose
  • StatusSub        : track battery/locomotion mode
  • Tick             : 5Hz behavior tree tick
  • WaypointPub      : (driven by tick) publish GoalPose to Go2
"""
from __future__ import annotations

import threading
import time
from typing import Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import (
    GoalPose,
    Header,
    Pose6D,
    RobotStatus,
    Waypoint,
)
from swarm.sector_assign import (
    V_FORMATION_MODE_DEFAULT,
    V_FORMATION_MODES,
    SectorAssignDispatcher,
    compute_v_formation_sectors,
    equal_split,
)

from .behavior_tree import (
    Action,
    Condition,
    Node,
    Sequence,
    Status,
)
from .leader_rollback import LeaderRollbackChecker
from .operational_modes import OperationalMode, OperationalModeController
from .patrol_planner import PatrolPlanner, PatrolRoute


class MissionContext:
    """Shared state for behavior tree leaves."""
    def __init__(self):
        self.current_pose: Optional[Pose6D] = None
        self.robot_status: Optional[RobotStatus] = None
        self.active_route: Optional[PatrolRoute] = None
        self.waypoint_idx: int = 0
        self.dwell_started_at: float = 0.0
        self.queues: TopicQueues = None
        self.cfg = None
        # Operational mode (RECON / NARROW / WIDE / ASSAULT / DEV_TEST).
        # Built in MissionProcess.setup(); BT leaves can read
        # ctx.mode_controller.get_max_speed() to cap velocity, etc.
        self.mode_controller: Optional[OperationalModeController] = None
        # Mirror of the auth proxy bool — convenient for BT condition
        # leaves that don't want to import OperationalModeController.
        self.pin_authenticated: bool = False


class MissionProcess(BaseProcess):
    def __init__(self, queues, shutdown_event, config,
                 auth_state_proxy=None, **diag):
        super().__init__(
            name="Mission",
            shutdown_event=shutdown_event,
            rate_hz=5.0,                 # BT tick rate
            cpu_affinity=config.get("system", "cpu_affinity", "mission"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._auth_proxy = auth_state_proxy
        self.planner: PatrolPlanner = None
        self.tree: Node = None
        self.ctx = MissionContext()
        self._lock: threading.Lock = None
        # Leader-rollback policy is loaded from config so an operator can
        # raise the threshold (e.g. 0.7) on terrain known to stress
        # followers transiently. Default 0.5 per SDD Rev.A.5 user
        # decision #6.
        self.rollback: LeaderRollbackChecker = None
        self._last_auth_seen: bool = False
        # Surveillance sector assignment (leader → robots). The dispatcher
        # is leader-only output; followers run a separate consumer in
        # PerceptionProcess. Leadership is tracked via sw_election_state.
        self._sector_dispatcher: Optional[SectorAssignDispatcher] = None
        self._is_swarm_leader: bool = False
        self._my_robot_id: int = 0
        self._sector_roster: tuple = ()

    def setup(self) -> None:
        self._lock = threading.Lock()
        routes_file = self.cfg.get("mission", "routes_file",
                                   default="config/patrol_routes.yaml")
        self.planner = PatrolPlanner(routes_file)
        self.planner.load()

        self.ctx.queues = self.queues
        self.ctx.cfg = self.cfg

        # Operational mode preset (SDD Rev.A.6 §7.1). Default to RECON;
        # an operator can switch via a future BLE/WS opcode once that
        # plumbing exists. DEV_TEST requires PIN auth — see step().
        self.ctx.mode_controller = OperationalModeController()
        default_mode = str(self.cfg.get("mission", "default_mode",
                                        default="recon")).lower()
        try:
            self.ctx.mode_controller.request_mode(
                OperationalMode(default_mode))
        except ValueError:
            self.log.warning(
                f"unknown mission.default_mode={default_mode!r}, keeping RECON")
        self.log.info(
            f"operational mode: {self.ctx.mode_controller.current.value} "
            f"(max_speed={self.ctx.mode_controller.get_max_speed()} m/s)")

        self.tree = self._build_tree()

        # P1-4 (S2): state-machine rollback checker. Constants are
        # inlined on the class (THRESHOLD_INITIATE / RETREAT_DURATION_S).
        # Config knobs are no longer wired; restore them here when an
        # operator needs runtime tuning.
        self.rollback = LeaderRollbackChecker()
        self.log.info(
            f"leader rollback armed: initiate={LeaderRollbackChecker.THRESHOLD_INITIATE:.2f} "
            f"retreat={LeaderRollbackChecker.RETREAT_DURATION_S:.0f}s")

        self.spawn_thread(self._pose_sub, name="PoseSub")
        self.spawn_thread(self._status_sub, name="StatusSub")

        # Sector-assignment publisher — leader-only. Roster + coverage
        # come from config; coverage_deg<360 limits sectors to a forward
        # arc when a rear watch is unwanted.
        self._my_robot_id = int(self.cfg.get("system", "robot_id", default=0))
        roster_cfg = self.cfg.get("swarm", "robot_ids", default=None)
        if roster_cfg:
            self._sector_roster = tuple(int(r) for r in roster_cfg)
        else:
            # Single-robot dev fallback: assign to ourselves.
            self._sector_roster = (self._my_robot_id,)
        self._sector_coverage_deg = float(self.cfg.get(
            "swarm", "sector_coverage_deg", default=360.0))
        # Policy switch — "equal_split" (default, role-agnostic) or
        # "v_formation" (standard 9-slot V-shape table with gap-fill +
        # threat-focus). v_formation requires `swarm.robot_roles` to map
        # role names → robot_ids; an unmapped/empty map yields no plans.
        self._sector_policy = str(self.cfg.get(
            "swarm", "sector_policy", default="equal_split")).lower()
        roles_cfg = self.cfg.get("swarm", "robot_roles", default=None) or {}
        self._sector_role_map = {
            str(role): int(rid) for role, rid in roles_cfg.items()
        }
        v_mode = str(self.cfg.get(
            "swarm", "v_formation_mode",
            default=V_FORMATION_MODE_DEFAULT)).lower()
        if v_mode not in V_FORMATION_MODES:
            self.log.warning(
                f"unknown swarm.v_formation_mode={v_mode!r}, falling back to "
                f"{V_FORMATION_MODE_DEFAULT!r}")
            v_mode = V_FORMATION_MODE_DEFAULT
        self._sector_v_mode = v_mode
        if self._sector_policy == "v_formation" and not self._sector_role_map:
            self.log.warning(
                "swarm.sector_policy=v_formation but swarm.robot_roles is "
                "empty — sector publisher will emit nothing")
        self._sector_dispatcher = SectorAssignDispatcher()
        self.spawn_thread(self._election_listener,        name="ElectionSub")
        self.spawn_thread(self._sector_assign_dispatch,   name="SectorPub")

    def step(self) -> None:
        # Refresh PIN-auth state from the cross-process proxy and push it
        # into the operational-mode controller. DEV_TEST mode can only be
        # entered while authenticated; gating happens inside request_mode().
        authed = bool(self._auth_proxy
                       and self._auth_proxy.get("authenticated", False))
        if self.ctx.mode_controller is not None:
            self.ctx.mode_controller.set_pin_authenticated(authed)
        self.ctx.pin_authenticated = authed
        if authed != self._last_auth_seen:
            self.log.info(
                f"mission PIN auth → {'AUTHED' if authed else 'NONE'}")
            self._last_auth_seen = authed

        # Schedule check (only when idle)
        if self.ctx.active_route is None:
            r = self.planner.due_route()
            if r is not None:
                self.log.info(f"patrol triggered: route={r.name} ({len(r.waypoints)} wp)")
                with self._lock:
                    self.ctx.active_route = r
                    self.ctx.waypoint_idx = 0
        # Tick behavior tree
        if self.ctx.active_route is not None:
            s = self.tree.tick(self.ctx)
            if s != Status.RUNNING:
                self.log.info(f"mission complete: {s.name}")
                publish(self.queues.mission_state, {
                    "status": s.name,
                    "route": self.ctx.active_route.name,
                    "mode": self.ctx.mode_controller.current.value
                            if self.ctx.mode_controller else "unknown",
                    "max_speed_mps": (
                        self.ctx.mode_controller.get_max_speed()
                        if self.ctx.mode_controller else None),
                })
                with self._lock:
                    self.ctx.active_route = None
                    self.ctx.waypoint_idx = 0

    # ─── consumers ───
    def _pose_sub(self):
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.05)
            if p is not None:
                self.ctx.current_pose = p

    def _status_sub(self):
        while self.is_running():
            st = consume(self.queues.robot_status, timeout=0.1)
            if st is not None:
                self.ctx.robot_status = st

    def _election_listener(self):
        """Track whether this robot is the elected swarm leader.

        sw_election_state carries Modified Raft elected_leader_id
        transitions; the sector dispatcher only emits when we are it.
        """
        while self.is_running():
            ev = consume(self.queues.sw_election_state, timeout=0.5)
            if ev is None:
                continue
            elected = getattr(ev, "elected_leader_id", None)
            if elected is None:
                elected = getattr(ev, "leader_id", None)
            was_leader = self._is_swarm_leader
            self._is_swarm_leader = (
                elected is not None and int(elected) == self._my_robot_id)
            if self._is_swarm_leader != was_leader:
                self.log.info(
                    f"swarm leader role → {'LEADER' if self._is_swarm_leader else 'FOLLOWER'} "
                    f"(elected={elected})")

    def _sector_assign_dispatch(self):
        """Periodic + event-driven SectorAssign publisher (leader-only).

        Tick at 2 Hz; the dispatcher applies its own 10 s cadence so most
        wakeups are no-ops. Publishing only when leader prevents
        duplicate fan-out from a stale ex-leader after a rollback.
        """
        while self.is_running():
            time.sleep(0.5)
            if not self._is_swarm_leader or self._sector_dispatcher is None:
                continue
            plans = self._compute_sector_plans()
            now_ms = int(time.time() * 1000)
            for msg in self._sector_dispatcher.due_messages(now_ms, plans):
                publish(self.queues.sw_sector_assign, msg)

    def _compute_sector_plans(self) -> list:
        """Dispatch to the configured sector policy.

        v_formation honours the standard 9-slot table + gap-fill +
        operating-mode bias, restricted to robots currently in roster
        (i.e. presumed alive). equal_split keeps the prior role-agnostic
        behavior for back-compat.
        """
        if self._sector_policy == "v_formation":
            return compute_v_formation_sectors(
                role_to_robot_id=self._sector_role_map,
                alive_robot_ids=self._sector_roster,
                threat_bearings_deg=(),  # threat-focus event hook is follow-up
                mode=self._sector_v_mode,
            )
        return equal_split(
            self._sector_roster,
            coverage_deg=self._sector_coverage_deg,
        )

    # ─── behavior tree ───
    def _build_tree(self) -> Node:
        return Sequence(
            Condition(self._cond_pose_available, name="PoseAvailable"),
            Condition(self._cond_battery_ok, name="BatteryOK"),
            Action(self._act_navigate_to_current_wp, name="NavigateToWaypoint"),
            Action(self._act_dwell_and_observe, name="DwellAndObserve"),
            Action(self._act_advance_or_finish, name="AdvanceWaypoint"),
            name="PatrolMission",
        )

    # ─── BT leaves ───
    def _cond_pose_available(self, ctx: MissionContext) -> bool:
        return ctx.current_pose is not None

    def _cond_battery_ok(self, ctx: MissionContext) -> bool:
        if ctx.robot_status is None:
            return True
        thresh = float(self.cfg.get("mission", "battery_threshold_return",
                                    default=0.30))
        return ctx.robot_status.battery_soc > thresh

    def _act_navigate_to_current_wp(self, ctx: MissionContext) -> Status:
        wp = ctx.active_route.waypoints[ctx.waypoint_idx]
        # Have we arrived?
        if self._distance_to(ctx.current_pose, wp) < 0.3:
            return Status.SUCCESS
        # Send goal pose (idempotent — Nav2 will track)
        goal = GoalPose(
            header=Header.now(frame_id="map"),
            position=wp.pose.position,
            orientation=wp.pose.orientation,
        )
        publish(ctx.queues.goal_pose, goal)
        return Status.RUNNING

    def _act_dwell_and_observe(self, ctx: MissionContext) -> Status:
        wp = ctx.active_route.waypoints[ctx.waypoint_idx]
        if ctx.dwell_started_at == 0.0:
            ctx.dwell_started_at = time.monotonic()
            self.log.info(f"dwell start at {wp.id} ({wp.dwell_sec}s)")
            return Status.RUNNING
        if time.monotonic() - ctx.dwell_started_at < wp.dwell_sec:
            return Status.RUNNING
        # done
        ctx.dwell_started_at = 0.0
        return Status.SUCCESS

    def _act_advance_or_finish(self, ctx: MissionContext) -> Status:
        ctx.waypoint_idx += 1
        if ctx.waypoint_idx >= len(ctx.active_route.waypoints):
            return Status.SUCCESS
        return Status.RUNNING

    @staticmethod
    def _distance_to(pose: Pose6D, wp: Waypoint) -> float:
        if pose is None:
            return float("inf")
        return float(np.linalg.norm(pose.position[:2] - wp.pose.position[:2]))
