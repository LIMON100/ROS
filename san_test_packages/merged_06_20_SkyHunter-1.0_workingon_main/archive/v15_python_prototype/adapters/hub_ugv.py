"""HubUgvAdapter — Hub UGV process (SDD Rev.A.6 §2.2, §6.5).

Three responsibilities, only on robot_role='hub':
  1. SLAM fusion: receive sw_follower_map (1 Hz from each follower),
     max-combine per-tile occupancy, broadcast as sw_shared_map (1 Hz).
  2. Comm gateway: relay DDS traffic + LTE backhaul fallback.
  3. Leader takeover: when elected via Modified Raft (P1-13),
     promote to acting leader.

Per-cell max fusion (np.max over per-robot occupancy):
  any robot detects an obstacle → broadcasted to the swarm.
"""
from __future__ import annotations

import threading
import time
from typing import Dict, Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import (
    THREAT_SEVERITY_WARNING,
    THREAT_TYPE_SBC_FAILED,
    MapTile,
    Pose2D,
    SwarmHealthSummary,
    ThreatAlert,
)
from mapping.aggregated_map import (
    AggregatedMapDispatcher,
    AggregatedMapInput,
)
from safety.hub_health_monitor import HubHealthMonitor


class HubUgvAdapter(BaseProcess):
    """Hub-role process. Spawned only when system.robot_role == "hub"."""

    LEADER_TIMEOUT_S = 5.0
    BROADCAST_PERIOD_S = 1.0   # 1 Hz fusion broadcast

    def __init__(self, queues: TopicQueues, shutdown_event, config, **diag):
        super().__init__(
            name="HubUgv",
            shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "swarm_bridge"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.role: str = "follower"
        # Always initialise the lock; downstream methods enter `with self._lock`
        # unconditionally, and the previous `None` value made every call from a
        # non-hub instance crash with `TypeError: __enter__` (no __enter__ on
        # NoneType). The lock costs nothing when no contender exists.
        self._lock: threading.Lock = threading.Lock()
        # Cache of follower SLAM tiles, keyed by tile_id (one per geometry).
        # Each value is { robot_id: MapTile } so the latest tile from each
        # robot drives the per-cell max.
        self._follower_tiles: Dict[str, Dict[str, MapTile]] = {}
        self._is_acting_leader: bool = False
        self._last_leader_seen: float = 0.0
        self._stats = {
            "follower_maps_in": 0,
            "shared_maps_out":  0,
            "leader_takeovers": 0,
            "aggregated_maps_out": 0,
            "peer_sbc_failures": 0,
            "peer_sbc_recoveries": 0,
            "swarm_health_summaries_out": 0,
        }
        # peer_id → role ("slam" | "comm"). Populated from
        # hub.peer_sbc_roles in setup(); empty in a single-SBC dev setup.
        # Used to set the right flag in SwarmHealthSummary.
        self._peer_sbc_role: Dict[str, str] = {}
        # Latched failure state per role; transitions drive the published
        # SwarmHealthSummary. Defaults: nothing failed.
        self._sbc_failed: Dict[str, bool] = {"slam": False, "comm": False}
        # Per-role peer_id that's currently latched as failed — needed so
        # a SwarmHealthSummary recovery message can name the right peer.
        self._sbc_failed_peer: Dict[str, str] = {"slam": "", "comm": ""}
        self._sbc_state_lock = threading.Lock()
        # Aggregated-map dispatcher; period_s is clamped to [30, 60] inside.
        self._agg_dispatcher: Optional[AggregatedMapDispatcher] = None
        # Peer SBC heartbeat surveillance — None until setup() reads the
        # roster from config. Tests that build this adapter via __new__
        # can construct one directly.
        self._hub_health: Optional[HubHealthMonitor] = None

    # ─── Lifecycle ───
    def setup(self) -> None:
        self.role = (self.cfg.get("system", "robot_role", default="follower")
                     or "follower").lower()
        if self.role != "hub":
            self.log.info(f"role={self.role} — HubUgvAdapter idle")
            return
        # _lock already initialised in __init__; no rebind needed.
        period = float(self.cfg.get(
            "hub", "aggregated_map_period_s", default=30.0))
        self._agg_dispatcher = AggregatedMapDispatcher(period_s=period)

        # Peer SBC heartbeat surveillance — roster defaults to the other
        # SBC of a dual-SBC pair. Empty roster disables the watchdog so
        # single-SBC dev setups don't constantly fire SBC_FAILED alerts.
        peer_ids = self.cfg.get("hub", "peer_sbc_ids", default=None) or []
        timeout = float(self.cfg.get(
            "hub", "peer_heartbeat_timeout_s", default=3.0))
        # peer_id → role mapping for SwarmHealthSummary flag dispatch.
        # Roles MUST be "slam" or "comm"; an unknown role is logged once
        # and ignored so a typo doesn't kill the whole monitor.
        roles_cfg = self.cfg.get("hub", "peer_sbc_roles", default=None) or {}
        for pid, role in dict(roles_cfg).items():
            role_str = str(role).lower()
            if role_str not in ("slam", "comm"):
                self.log.warning(
                    f"hub.peer_sbc_roles[{pid!r}]={role!r} — expected "
                    "'slam' or 'comm', ignored")
                continue
            self._peer_sbc_role[str(pid)] = role_str
        if peer_ids:
            self._hub_health = HubHealthMonitor(
                peer_ids=peer_ids, timeout_sec=timeout)
            self.spawn_thread(self._hub_health_tick, name="HubHealthTick")
            self.log.info(
                f"hub health monitor armed: peers={list(peer_ids)} "
                f"timeout={timeout:.1f}s roles={dict(self._peer_sbc_role)}")
        else:
            self.log.info("hub health monitor disabled (no peer_sbc_ids)")

        self.spawn_thread(self._follower_map_consumer,  name="HubFollowerMap")
        self.spawn_thread(self._election_listener,      name="HubElection")
        self.spawn_thread(self._aggregated_map_broadcast,
                          name="HubAggMapPub")

    def step(self) -> None:
        if self.role != "hub":
            return
        self._fuse_and_broadcast()
        s = self._stats
        self.log.info(
            f"hub  fmaps_in={s['follower_maps_in']} "
            f"shared_out={s['shared_maps_out']} "
            f"takeovers={s['leader_takeovers']}")

    # ─── Public hooks (used directly by tests) ───
    def slam_fusion_consume(self, tile: MapTile) -> None:
        """Inject one follower MapTile (test hook + thread bridge)."""
        with self._lock:
            self._follower_tiles.setdefault(tile.tile_id, {})[
                str(tile.source)] = tile
            self._stats["follower_maps_in"] += 1

    def fuse_tiles(self) -> list[MapTile]:
        """Per-tile max-fusion. Returns the list of fused MapTiles ready
        to broadcast. Pure logic — no IPC, callable from tests."""
        out: list[MapTile] = []
        with self._lock:
            for tid, by_source in self._follower_tiles.items():
                tiles = list(by_source.values())
                if not tiles:
                    continue
                ref = tiles[0]
                shape = ref.occupancy.shape
                stack = []
                for t in tiles:
                    if t.occupancy.shape == shape:
                        stack.append(t.occupancy)
                if not stack:
                    continue
                fused_occ = np.stack(stack).max(axis=0).astype(np.float32)
                out.append(MapTile(
                    tile_id=tid,
                    origin_xy=ref.origin_xy,
                    size_m=ref.size_m,
                    resolution=ref.resolution,
                    occupancy=fused_occ,
                    confidence=1.0,
                    source="hub_fused",
                    last_update=time.time(),
                    persistence=ref.persistence,
                ))
        return out

    def build_aggregated_input(self) -> Optional[AggregatedMapInput]:
        """Construct one AggregatedMapInput from the current fused tiles.

        Picks the most recently updated fused tile as the broadcast grid
        and counts the distinct robot IDs that have ever contributed to
        any tile in this hub's cache (the "contributing_robots" field).
        Returns None when no fused tile is available yet.
        """
        fused = self.fuse_tiles()
        if not fused:
            return None
        # Most recently updated fused tile.
        chosen = max(fused, key=lambda t: t.last_update)
        with self._lock:
            contributors = set()
            for by_source in self._follower_tiles.values():
                contributors.update(by_source.keys())
        origin = Pose2D(
            x=float(chosen.origin_xy[0]),
            y=float(chosen.origin_xy[1]),
            theta_rad=0.0,
        )
        return AggregatedMapInput(
            grid=chosen.occupancy,
            origin=origin,
            resolution_m=float(chosen.resolution),
            contributing_robots=len(contributors),
        )

    def record_peer_heartbeat(
        self, peer_id: str, *, now: Optional[float] = None,
    ) -> bool:
        """Pulse a peer SBC's heartbeat into the watchdog.

        Returns True iff this beat recovers a previously-stale peer (so
        the caller can fire a follow-up "clear" alert if desired). When
        the monitor is disabled (no peer roster), this is a no-op.
        Intended call sites: DDS bridge inbound thread on every
        liveliness assertion from a peer SBC; tests may call directly.

        On recovery, also clears the SBC role flag in the latched
        SwarmHealthSummary state and publishes a fresh summary.
        """
        if self._hub_health is None:
            return False
        recovered = self._hub_health.record_heartbeat(peer_id, now=now)
        if recovered:
            self._stats["peer_sbc_recoveries"] += 1
            self.log.info(f"hub peer SBC#{peer_id} recovered")
            self._update_sbc_role_state(
                peer_id=str(peer_id), failed=False)
        return recovered

    # ─── SBC failure state machine ───
    def _role_for_peer(self, peer_id: str) -> str:
        """Look up the role ('slam'|'comm') for a peer, or '' if unmapped."""
        return self._peer_sbc_role.get(str(peer_id), "")

    def _update_sbc_role_state(
        self, *, peer_id: str, failed: bool,
    ) -> None:
        """Latch a role's failure flag and publish a SwarmHealthSummary.

        Publishes only on a real transition (idempotent re-flagging is
        a no-op) so an outage that retriggers `check_timeouts` doesn't
        spam the queue. An unmapped peer_id is logged at debug and
        produces no summary — the operator still sees the ThreatAlert.
        """
        role = self._role_for_peer(peer_id)
        if role not in ("slam", "comm"):
            self.log.debug(
                f"hub peer {peer_id!r} has no SBC role mapping — "
                "skipping SwarmHealthSummary publish")
            return
        with self._sbc_state_lock:
            if self._sbc_failed[role] == failed:
                return                     # idempotent — no transition
            self._sbc_failed[role] = failed
            self._sbc_failed_peer[role] = str(peer_id) if failed else ""
            summary = SwarmHealthSummary(
                slam_sbc_failed=self._sbc_failed["slam"],
                comm_sbc_failed=self._sbc_failed["comm"],
                slam_sbc_peer_id=self._sbc_failed_peer["slam"],
                comm_sbc_peer_id=self._sbc_failed_peer["comm"],
                timestamp_ms=int(time.time() * 1000),
            )
            summary.validate()
        publish(self.queues.hub_swarm_health_summary, summary)
        self._stats["swarm_health_summaries_out"] += 1
        self.log.info(
            f"hub SwarmHealthSummary published: "
            f"slam_failed={summary.slam_sbc_failed} "
            f"comm_failed={summary.comm_sbc_failed} (transition on "
            f"role={role}, peer={peer_id})")

    def note_leader_heartbeat(self, *, now: Optional[float] = None) -> None:
        """Record a leader heartbeat — caller from DDS relay or test."""
        with self._lock:
            self._last_leader_seen = (now if now is not None
                                      else time.monotonic())

    def leader_takeover_handler(self, *,
                                 now: Optional[float] = None) -> bool:
        """Decide whether to assume leadership. Returns True iff this
        call triggered a fresh transition into acting-leader state."""
        cur = now if now is not None else time.monotonic()
        with self._lock:
            last = self._last_leader_seen
            silent_for = cur - last if last > 0 else float("inf")
            if (not self._is_acting_leader
                    and silent_for >= self.LEADER_TIMEOUT_S):
                self._is_acting_leader = True
                self._stats["leader_takeovers"] += 1
                self.log.warning(
                    f"hub leader takeover — no leader heartbeat for "
                    f"{silent_for:.1f}s (≥{self.LEADER_TIMEOUT_S:.1f}s)")
                return True
            if self._is_acting_leader and silent_for < self.LEADER_TIMEOUT_S:
                self._is_acting_leader = False
        return False

    def is_acting_leader(self) -> bool:
        with self._lock:
            return self._is_acting_leader

    # ─── Internal threads ───
    def _follower_map_consumer(self) -> None:
        """Drain sw_follower_map into the merger."""
        while self.is_running():
            tile = consume(self.queues.sw_follower_map, timeout=0.2)
            if tile is None:
                continue
            self.slam_fusion_consume(tile)

    def _fuse_and_broadcast(self) -> None:
        for fused in self.fuse_tiles():
            publish(self.queues.sw_shared_map, fused)
            self._stats["shared_maps_out"] += 1

    def _hub_health_tick(self) -> None:
        """1 Hz peer-SBC timeout check.

        Fires one ThreatAlert(SBC_FAILED, WARNING) per peer that has
        just gone stale; de-dup is handled inside HubHealthMonitor so
        an outage doesn't flood the alert queue. The message_ko mirrors
        the operator UI string from the spec.
        """
        while self.is_running():
            time.sleep(1.0)
            if self._hub_health is None:
                continue
            for pid in self._hub_health.check_timeouts():
                alert = ThreatAlert(
                    severity=THREAT_SEVERITY_WARNING,
                    threat_type=THREAT_TYPE_SBC_FAILED,
                    message_ko=f"Hub UGV SBC #{pid} 응답 없음 — 부분 운용 진입",
                    peer_id=str(pid),
                    timestamp_ms=int(time.time() * 1000),
                )
                publish(self.queues.hub_threat_alert, alert)
                self._stats["peer_sbc_failures"] += 1
                self.log.warning(
                    f"hub peer SBC#{pid} silent past timeout — alert published")
                # Latch the SBC role flag and publish a typed
                # SwarmHealthSummary so downstream consumers see the
                # steady-state degraded mode, not just the edge alert.
                self._update_sbc_role_state(peer_id=str(pid), failed=True)

    def _aggregated_map_broadcast(self) -> None:
        """Hub-only periodic publish of the fused global map.

        Ticks at 1 Hz; the dispatcher applies its own 30–60 s cadence so
        most wakeups are no-ops. Pillow availability is checked lazily
        inside the codec — an environment without Pillow logs once and
        keeps the rest of the hub running.
        """
        encode_warned = False
        while self.is_running():
            time.sleep(1.0)
            if self._agg_dispatcher is None:
                continue
            inputs = self.build_aggregated_input()
            now_ms = int(time.time() * 1000)
            try:
                msg = self._agg_dispatcher.due_message(now_ms, inputs)
            except RuntimeError as e:
                if not encode_warned:
                    self.log.warning(
                        f"aggregated-map encoding unavailable: {e}")
                    encode_warned = True
                continue
            if msg is None:
                continue
            publish(self.queues.hub_aggregated_map, msg)
            self._stats["aggregated_maps_out"] += 1

    def _election_listener(self) -> None:
        """Watch sw_election_state for elected_leader_id transitions and
        flip into acting-leader mode when this hub is the elected one."""
        while self.is_running():
            ev = consume(self.queues.sw_election_state, timeout=0.5)
            if ev is None:
                # Also tick the silent-leader takeover heuristic.
                self.leader_takeover_handler()
                continue
            elected = getattr(ev, "elected_leader_id", None)
            if elected is not None and elected == self._my_robot_id():
                with self._lock:
                    if not self._is_acting_leader:
                        self._is_acting_leader = True
                        self._stats["leader_takeovers"] += 1
                        self.log.info(
                            f"Hub UGV elected — robot_id={elected}")

    def _my_robot_id(self) -> int:
        return int(self.cfg.get("system", "robot_id", default=0))
