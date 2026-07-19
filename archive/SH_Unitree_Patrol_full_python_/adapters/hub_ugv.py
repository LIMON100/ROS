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
from core.messages import MapTile


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
        }

    # ─── Lifecycle ───
    def setup(self) -> None:
        self.role = (self.cfg.get("system", "robot_role", default="follower")
                     or "follower").lower()
        if self.role != "hub":
            self.log.info(f"role={self.role} — HubUgvAdapter idle")
            return
        # _lock already initialised in __init__; no rebind needed.
        self.spawn_thread(self._follower_map_consumer, name="HubFollowerMap")
        self.spawn_thread(self._election_listener,     name="HubElection")

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
