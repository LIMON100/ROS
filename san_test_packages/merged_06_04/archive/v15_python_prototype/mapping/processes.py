"""Map-related processes (post HD→OSM transition, SDD Rev.A.6):
  • SLAMBridgeProcess: receive Point-LIO output → CumulativeMap + slam tile pub
  • MapFusionProcess : republish SLAM tiles as fused; host OSM static and
                       SLAM persistent layers for downstream anomaly queries
                       (3-layer cost-map: §4.7.1, §4.7.5)
"""
from __future__ import annotations

import threading
import time
from typing import Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import MapTile, Pose6D
from core.shm_pool import ShmPool

from .cumulative_map import CumulativeMap
from .osm_static_layer import OsmStaticLayer
from .slam_persistent_layer import SlamPersistentLayer


def is_hub_mode(config) -> bool:
    """True when `system.robot_role == "hub"`. Hub mode keeps MapFusion's
    single-robot path off — fusion is performed by HubUgvAdapter over the
    merged sw/follower_map stream instead."""
    role = config.get("system", "robot_role", default="follower")
    return str(role).lower() == "hub"


# ─────────────────── SLAMBridgeProcess ───────────────────
class SLAMBridgeProcess(BaseProcess):
    """
    Bridge to external SLAM (Point-LIO ROS2 node).
    We subscribe to its pose output (or run own light scan-matcher),
    and feed accumulated points into CumulativeMap.

    For now: stub that consumes our own lidar_ref + pose to update cumulative map.
    Real impl: subscribe to /Odometry from Point-LIO, transform points to world,
    feed CumulativeMap.update().
    """
    def __init__(self, queues, shutdown_event, config, lidar_shm: ShmPool, **diag):
        super().__init__(
            name="SLAMBridge",
            shutdown_event=shutdown_event,
            rate_hz=1.0,
            cpu_affinity=config.get("system", "cpu_affinity", "slam_bridge"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.lidar_shm = lidar_shm
        self.cum_map = CumulativeMap()
        self._lock: threading.Lock = None
        self._latest_pose: Pose6D = None
        self._stats = {"scans": 0, "tiles_pub": 0}

    def setup(self) -> None:
        self._lock = threading.Lock()
        self.spawn_thread(self._pose_consumer, name="PoseSub")
        self.spawn_thread(self._lidar_accumulator, name="LidarAccum")
        self.spawn_thread(self._tile_publisher, name="TilePub")

    def step(self) -> None:
        self.log.info(
            f"slam_bridge  scans={self._stats['scans']} tiles={self._stats['tiles_pub']}"
        )

    def _pose_consumer(self) -> None:
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.05)
            if p is not None:
                with self._lock:
                    self._latest_pose = p

    def _lidar_accumulator(self) -> None:
        while self.is_running():
            ref = consume(self.queues.lidar_ref, timeout=0.1)
            if ref is None:
                continue
            try:
                shm = ShmPool.attach(ref.shm_name)
                pts = ShmPool.view_array(shm, (ref.n_points, 4), np.float32)
                # transform to world frame using latest pose
                with self._lock:
                    pose = self._latest_pose
                if pose is not None:
                    world_xyz = pts[:, :3] + pose.position[None, :]   # simplified
                else:
                    world_xyz = pts[:, :3]
                self.cum_map.update(world_xyz, sensor_xy=(0.0, 0.0))
                self._stats["scans"] += 1
                shm.close()
            except FileNotFoundError:
                pass
            finally:
                self.lidar_shm.release(ref.shm_name)

    def _tile_publisher(self) -> None:
        """Periodically publish updated cumulative tiles for fusion."""
        while self.is_running():
            time.sleep(1.0)
            with self.cum_map._ensure_lock():
                tiles_idx = list(self.cum_map._tiles.keys())
            for (ix, iy) in tiles_idx:
                pers = self.cum_map.persistence(ix, iy)
                tile = MapTile(
                    tile_id=f"slam_{ix}_{iy}",
                    origin_xy=(ix * self.cum_map.tile_size_m,
                               iy * self.cum_map.tile_size_m),
                    size_m=self.cum_map.tile_size_m,
                    resolution=self.cum_map.resolution,
                    occupancy=(pers > 0.5).astype(np.float32),
                    confidence=1.0,
                    source="slam",
                    last_update=time.time(),
                    persistence=pers,
                )
                publish(self.queues.cumulative_update, tile)
                self._stats["tiles_pub"] += 1
                # Mirror to sw_follower_map so a Hub UGV (if present) can
                # consume the multi-robot stream for fusion. No-op when
                # the topic isn't wired up (older test fixtures).
                fmap_q = getattr(self.queues, "sw_follower_map", None)
                if fmap_q is not None:
                    publish(fmap_q, tile)


# ─────────────────── MapFusionProcess ───────────────────
class MapFusionProcess(BaseProcess):
    """Republish SLAM tiles as fused; host OSM static + SLAM persistent
    layers for downstream anomaly queries.

    With the HD-map path retired (SDD Rev.A.6), tile blending collapses
    into a passthrough — the OSM static layer is held at process scope so
    callers can run detect_anomaly() against a tile-aligned region of the
    persistent layer. Cell-level Bayesian fusion of SLAM tiles into
    persistent_layer is deferred until PBF loading lands.
    """

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="MapFusion",
            shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "map_fusion"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._lock: threading.Lock = None
        self._latest_pose: Optional[Pose6D] = None
        self._slam_buf: list = []
        self._fused = 0
        # 3-layer cost-map components — instantiated in setup() so unit
        # tests that import the class don't pay the construction cost.
        self.osm_layer: Optional[OsmStaticLayer] = None
        self.persistent_layer: Optional[SlamPersistentLayer] = None

    def setup(self) -> None:
        self._lock = threading.Lock()
        self.osm_layer = OsmStaticLayer()
        self.persistent_layer = SlamPersistentLayer()
        self.spawn_thread(self._pose_sub,      name="PoseSub")
        self.spawn_thread(self._slam_consumer, name="SlamCnsm")

    def step(self) -> None:
        with self._lock:
            buf = self._slam_buf
            self._slam_buf = []
        for slam in buf:
            slam.source = "fused"
            publish(self.queues.fused_tile, slam)
            self._fused += 1

    def _pose_sub(self):
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.05)
            if p is not None:
                self._latest_pose = p

    def _slam_consumer(self):
        while self.is_running():
            t = consume(self.queues.cumulative_update, timeout=0.1)
            if t is None:
                continue
            with self._lock:
                self._slam_buf.append(t)

    @staticmethod
    def detect_anomaly(static_layer, persistent_layer,
                       threshold: float = 0.7):
        """Detect cells where OSM and SLAM disagree (SDD Rev.A.6 §4.7.5).

        Returns a list of dicts with keys: type, x_idx, y_idx, confidence.

        Categories:
          - unmapped_obstacle: OSM=Free (<0.3), SLAM=Occupied (>threshold)
                               (vehicle, temp barrier, parked equipment)
          - structure_changed: OSM=Building (>0.7), SLAM=Free (<0.3)
                               (demolished structure or stale OSM)
          - road_blocked:      OSM=Road (~0.2), SLAM=Occupied (>threshold)
                               (checkpoint, fallen tree)
        """
        if static_layer.grid is None or persistent_layer.grid is None:
            return []
        if static_layer.grid.shape != persistent_layer.grid.shape:
            return []

        s = static_layer.grid       # OSM
        p = persistent_layer.grid   # SLAM

        anomalies = []

        # Case 1: OSM free but SLAM occupied
        mask = (s < 0.3) & (p > threshold)
        for iy, ix in zip(*np.where(mask), strict=False):
            anomalies.append({
                "type": "unmapped_obstacle",
                "x_idx": int(ix), "y_idx": int(iy),
                "confidence": float(p[iy, ix] - s[iy, ix]),
            })

        # Case 2: OSM building but SLAM free
        mask = (s > 0.7) & (p < 0.3)
        for iy, ix in zip(*np.where(mask), strict=False):
            anomalies.append({
                "type": "structure_changed",
                "x_idx": int(ix), "y_idx": int(iy),
                "confidence": float(s[iy, ix] - p[iy, ix]),
            })

        # Case 3: OSM road (~0.2) but SLAM occupied
        mask = (s > 0.15) & (s < 0.3) & (p > threshold)
        for iy, ix in zip(*np.where(mask), strict=False):
            anomalies.append({
                "type": "road_blocked",
                "x_idx": int(ix), "y_idx": int(iy),
                "confidence": float(p[iy, ix] - 0.2),
            })

        return anomalies
