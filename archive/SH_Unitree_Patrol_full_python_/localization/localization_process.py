"""
LocalizationProcess — produces the final localized pose for mission/control.

Fusion priority (3-tier):
    1. RTK Fixed   (~2 cm)        — preferred whenever available
    2. RTK Float   (~30 cm)       — accepted with reduced confidence
    3. Odometry    (drift-prone)  — fallback when RTK unavailable

When RTK is lost, the pose comes from a DeadReckoner that integrates IMU
samples from the latest known-good pose. This is more accurate than just
echoing SLAM's pose verbatim, because it gives us a clean drift estimate
tied to actual motion and a defined uncertainty model.

Inputs:
    • RtkFix         from RtkGnssAdapter        (5–10 Hz, intermittent)
    • Pose hint      from SLAM/Point-LIO        (10 Hz, drifts over time)
    • ImuData        from external IMU          (200 Hz, prior for prediction)
    • Fused tiles    from MapFusionProcess      (used for scan-match later)

Outputs:
    • Pose6D                  on queues.pose                  (100 Hz)
    • LocalizationStatus      on queues.localization_status   (every cycle)

Behavior contract (verified by tests):
    • RTK lost (>rtk_max_age_s) ⇒ status.source = 'odometry' within 1 cycle
    • RTK reacquired           ⇒ status.source returns to 'rtk_*' immediately;
                                  DeadReckoner reset() snaps state to the fix
    • During odometry-only mode, pose comes from DeadReckoner integration of
      IMU since the last good fix; covariance grows with distance traveled.
"""
from __future__ import annotations

import logging
import threading
import time
from dataclasses import dataclass
from typing import Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import (
    RTK_FIX_FIXED,
    RTK_FIX_FLOAT,
    Header,
    ImuData,
    LocalizationStatus,
    MapTile,
    Pose6D,
    RtkFix,
)

from .dead_reckoning import DeadReckoner, DrConfig

log = logging.getLogger(__name__)


@dataclass
class _LocConfig:
    rtk_max_age_s: float       = 1.5      # >1.5 s of stale RTK → fall back
    rtk_fixed_sigma_max: float = 0.10     # m, accept Fixed only below this
    odom_drift_per_s: float    = 0.02     # m/s assumed drift when RTK lost
    odom_drift_per_m: float    = 0.01     # m per meter traveled (DR-specific)


class LocalizationProcess(BaseProcess):
    """
    Threading model:
      Three consumer threads update shared state under self._lock.
      The main loop reads consistent snapshot and publishes pose + status.
    """

    def __init__(self, queues: TopicQueues, shutdown_event, config, **diag):
        super().__init__(
            name="Localization",
            shutdown_event=shutdown_event,
            rate_hz=100.0,
            cpu_affinity=config.get("system", "cpu_affinity", "localization"),
            rt_priority=config.get("system", "rt_priority", "localization") or 0,
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._lcfg = _LocConfig(
            rtk_max_age_s=config.get("localization", "rtk_max_age_s", default=1.5),
            rtk_fixed_sigma_max=config.get("localization",
                                           "rtk_fixed_sigma_max", default=0.10),
            odom_drift_per_s=config.get("localization",
                                        "odom_drift_per_s", default=0.02),
            odom_drift_per_m=config.get("localization",
                                        "odom_drift_per_m", default=0.01),
        )

        # Shared state (protected by _lock)
        self._lock: Optional[threading.Lock] = None
        self._latest_rtk: Optional[RtkFix] = None
        self._latest_slam: Optional[Pose6D] = None
        self._latest_imu: Optional[ImuData] = None
        self._fused: dict[str, MapTile] = {}

        # Last good RTK time — used to estimate odometry drift
        self._last_rtk_good_t: float = 0.0
        self._x = np.zeros(3, dtype=np.float32)
        self._q = np.array([0, 0, 0, 1], dtype=np.float32)
        self._sigma_xy = 99.0
        self._seq = 0
        self._status_seq = 0

        # Dead reckoner — drives pose during RTK outages
        self._dr = DeadReckoner(cfg=DrConfig())
        self._dr_active = False           # are we currently using DR output?

    def setup(self) -> None:
        self._lock = threading.Lock()
        self.spawn_thread(self._rtk_consumer,        name="RtkSub")
        self.spawn_thread(self._slam_consumer,       name="SlamSub")
        self.spawn_thread(self._imu_consumer,        name="ImuSub")
        self.spawn_thread(self._fused_tile_consumer, name="TileSub")

    # ────────── Main publish loop ──────────
    def step(self) -> None:
        now = time.monotonic()
        with self._lock:
            source, reason, sigma = self._select_source(now)
            x = self._x.copy()
            q = self._q.copy()
            self._sigma_xy = sigma
            rtk_age = (now - self._latest_rtk.header.stamp) \
                      if self._latest_rtk is not None else 99.0
            odom_drift = self._dr.distance_since_reset

        # Publish pose
        cov = np.zeros((6, 6), dtype=np.float32)
        cov[0, 0] = sigma * sigma
        cov[1, 1] = sigma * sigma
        cov[2, 2] = (sigma * 1.5) ** 2
        publish(self.queues.pose, Pose6D(
            header=Header.now(frame_id="map", seq=self._seq),
            position=x, orientation=q, covariance=cov,
        ))
        self._seq += 1

        # Publish localization status
        publish(self.queues.localization_status, LocalizationStatus(
            header=Header.now(frame_id="map", seq=self._status_seq),
            source=source, fallback_reason=reason,
            rtk_age_s=float(rtk_age),
            odom_drift_m=float(odom_drift),
            sigma_xy=float(sigma),
        ))
        self._status_seq += 1

    # ────────── Source selection (the heart of fallback logic) ──────────
    def _select_source(self, now: float):
        """Update self._x/_q based on best available source.

        Returns (source_name, fallback_reason, sigma_xy).
        """
        rtk = self._latest_rtk
        rtk_age = (now - rtk.header.stamp) if rtk else 99.0

        # ── Tier 1: RTK Fixed (cm-level) ──
        if rtk is not None \
                and rtk.fix_quality == RTK_FIX_FIXED \
                and rtk.sigma_xy <= self._lcfg.rtk_fixed_sigma_max \
                and rtk_age <= self._lcfg.rtk_max_age_s:
            self._x[:] = rtk.enu
            if self._latest_slam is not None:
                self._q[:] = self._latest_slam.orientation
            # Snap dead-reckoner to truth for the next outage
            self._dr.reset(rtk.enu.astype(np.float64), self._q.astype(np.float64))
            self._dr_active = False
            self._last_rtk_good_t = now
            return "rtk_fixed", "", rtk.sigma_xy

        # ── Tier 2: RTK Float (~30 cm) ──
        if rtk is not None \
                and rtk.fix_quality == RTK_FIX_FLOAT \
                and rtk_age <= self._lcfg.rtk_max_age_s:
            self._x[:] = rtk.enu
            if self._latest_slam is not None:
                self._q[:] = self._latest_slam.orientation
            self._dr.reset(rtk.enu.astype(np.float64), self._q.astype(np.float64))
            self._dr_active = False
            self._last_rtk_good_t = now
            return "rtk_float", "", rtk.sigma_xy

        # ── Tier 3: Odometry fallback ──
        # If we have a previously-good RTK fix, the DeadReckoner has been
        # seeded — use its IMU-integrated pose. Otherwise (cold start, RTK
        # never seen) fall back to whatever SLAM gives us.
        slam = self._latest_slam
        dr_seeded = self._last_rtk_good_t > 0 and self._dr.last_t is not None
        if dr_seeded:
            self._dr_active = True
            dr_pos, dr_q = self._dr.pose()
            self._x[:] = dr_pos
            self._q[:] = dr_q
            time_since_rtk = now - self._last_rtk_good_t
            sigma = (0.30
                     + self._lcfg.odom_drift_per_s * time_since_rtk
                     + self._lcfg.odom_drift_per_m * self._dr.distance_since_reset)
        elif slam is not None:
            # Either RTK never came in, OR the IMU never produced a sample —
            # fall back to SLAM pose verbatim
            self._x[:] = slam.position
            self._q[:] = slam.orientation
            time_since_rtk = (now - self._last_rtk_good_t) \
                             if self._last_rtk_good_t > 0 else now
            sigma = 0.30 + self._lcfg.odom_drift_per_s * time_since_rtk
        else:
            sigma = 99.0

        if rtk is None:
            reason = "rtk_never_received"
        elif rtk_age > self._lcfg.rtk_max_age_s:
            reason = f"rtk_stale_{rtk_age:.1f}s"
        elif rtk.fix_quality not in (RTK_FIX_FIXED, RTK_FIX_FLOAT):
            reason = f"rtk_no_fix_q={rtk.fix_quality}"
        else:
            reason = f"rtk_sigma_too_high_{rtk.sigma_xy:.2f}m"

        if dr_seeded:
            return "odometry", reason, float(sigma)
        if slam is not None:
            return "odometry", reason, float(sigma)
        return "dead_reckoning", reason, float(sigma)

    # ────────── Consumer threads ──────────
    def _rtk_consumer(self):
        while self.is_running():
            msg = consume(self.queues.rtk, timeout=0.1)
            if msg is not None:
                with self._lock:
                    self._latest_rtk = msg

    def _slam_consumer(self):
        # SLAMBridge publishes its incremental pose hint on cumulative_update.
        # (We don't subscribe to /pose because that's our OWN output.)
        while self.is_running():
            msg = consume(self.queues.cumulative_update, timeout=0.1)
            if msg is not None and hasattr(msg, "position"):
                with self._lock:
                    self._latest_slam = msg

    def _imu_consumer(self):
        while self.is_running():
            # Prefer external IMU (higher quality); fall back to Go2 internal
            d = consume(self.queues.imu_external, timeout=0.01)
            if d is None:
                d = consume(self.queues.imu, timeout=0.01)
            if d is not None:
                with self._lock:
                    self._latest_imu = d
                    # Drive dead-reckoner with every IMU sample so it stays
                    # ready to take over on the next RTK outage
                    self._dr.step(
                        t=d.header.stamp,
                        accel_body=d.linear_acc.astype(np.float64),
                        gyro=d.angular_vel.astype(np.float64),
                    )

    def _fused_tile_consumer(self):
        while self.is_running():
            tile = consume(self.queues.fused_tile, timeout=0.1)
            if tile is None:
                continue
            with self._lock:
                self._fused[tile.tile_id] = tile
