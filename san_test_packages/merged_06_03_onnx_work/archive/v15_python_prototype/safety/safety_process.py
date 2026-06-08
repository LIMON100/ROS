"""
SafetyProcess: monitors for E1-E6 exceptions.

Threads:
  • StatusSub        : robot_status updates
  • ImuSub           : imu updates
  • LocSub           : localization_status updates
  • RtkSub           : rtk_fix updates (for geofence)
  • FallDetector     : E3 — IMU acc spike

step() decisions (rate_hz=2):
  E1 — comm loss (placeholder, CommProcess feeds heartbeat)
  E2 — battery via BatteryMonitor 3-tier ladder (WARN/RTH/EMERGENCY)
  E3 — fall (separate fast thread)
  E4 — localization degraded
  E5 — fault codes
  E6 — geofence APPROACHING / VIOLATION (PIN-auth dev_override skips)

E2 and E6 honor PIN-authenticated dev_override propagated from BLE via
the auth_state proxy (see main._auth_state_relay). When the operator
has authenticated, BatteryMonitor and Geofence suppress actions so a
developer can drain a battery / wander outside the fence for testing
without the robot bolting for the charger or halting on a bench.
"""
from __future__ import annotations

import threading
import time
from typing import Optional

import numpy as np

from core.audit_log import publish_audit
from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import (
    LTE_NOT_REGISTERED,
    RTK_FIX_FIXED,
    RTK_FIX_FLOAT,
    Header,
    ImuData,
    RobotStatus,
    RtkFix,
    SafetyEvent,
)

from .battery_monitor import BatteryAction, BatteryMonitor
from .geofence import FenceState, Geofence
from .health_monitor import ComponentState, HealthMonitor, HealthState


class SafetyProcess(BaseProcess):
    def __init__(self, queues, shutdown_event, config,
                 auth_state_proxy=None, **diag):
        super().__init__(
            name="Safety",
            shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "safety"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._auth_proxy = auth_state_proxy
        self._lock: threading.Lock = None
        self._latest_status: RobotStatus = None
        self._latest_imu: ImuData = None
        self._latest_loc_status = None
        self._latest_rtk: Optional[RtkFix] = None
        self._lidar_last_seen: float = 0.0
        self._last_heartbeat: float = 0.0
        # Edge-trigger guards so we don't spam events every cycle
        self._loc_degraded_emitted = False
        self._geofence_state: FenceState = FenceState.SAFE
        self._last_auth_seen: bool = False
        self._last_health_state: HealthState = HealthState.NORMAL
        # Cached gate modules — built in setup() so the cfg/log are ready.
        self._battery: Optional[BatteryMonitor] = None
        self._geofence: Optional[Geofence] = None
        self._health: Optional[HealthMonitor] = None
        self._stats = {"events": 0}

    def setup(self) -> None:
        self._lock = threading.Lock()
        self._last_heartbeat = time.monotonic()

        # ── BatteryMonitor — replaces inline E2 threshold check.
        # The constructor still defaults dev_override=False; we update
        # it from the auth proxy on every step(). on_action fires once
        # per ladder transition and is what publishes the SafetyEvent.
        self._battery = BatteryMonitor(on_action=self._on_battery_action)
        # Allow YAML overrides of the class-level thresholds without
        # changing BatteryMonitor's constructor surface (its existing
        # unit tests rely on the defaults).
        warn = self.cfg.get("safety", "battery_warn_pct", default=None)
        rth = self.cfg.get("safety", "battery_rth_pct", default=None)
        emerg = self.cfg.get("safety", "battery_emergency_pct", default=None)
        if warn is not None:
            self._battery.WARN_THRESHOLD = float(warn)
        if rth is not None:
            self._battery.RTH_THRESHOLD = float(rth)
        if emerg is not None:
            self._battery.EMERGENCY_THRESHOLD = float(emerg)
        self.log.info(
            f"BatteryMonitor armed: WARN<{self._battery.WARN_THRESHOLD:.0f}% "
            f"RTH≤{self._battery.RTH_THRESHOLD:.0f}% "
            f"EMERG≤{self._battery.EMERGENCY_THRESHOLD:.0f}%")

        # ── Geofence — optional. When no polygon is configured (typical
        # in lab/dev), Geofence stays None and the E6 block is a no-op.
        polygon = self.cfg.get("safety", "geofence_polygon", default=None)
        if polygon:
            try:
                self._geofence = Geofence(
                    polygon=[tuple(p) for p in polygon],
                    buffer_m=float(self.cfg.get("safety",
                                                "geofence_buffer_m",
                                                default=2.0)),
                    hard_stop_m=float(self.cfg.get("safety",
                                                   "geofence_hard_stop_m",
                                                   default=0.5)),
                )
                self.log.info(
                    f"Geofence armed: {len(polygon)} verts, "
                    f"buffer={self._geofence.buffer_m}m "
                    f"hard_stop={self._geofence.hard_stop_m}m")
            except ValueError as e:
                # < 3 polygon vertices — log and run without a fence rather
                # than crash the safety process at boot.
                self.log.warning(f"Geofence disabled: {e}")
                self._geofence = None
        else:
            self.log.info("Geofence: no polygon configured — disabled")

        # ── HealthMonitor — system-wide component roll-up. report() is
        # called from each subscriber as fresh data arrives; step() then
        # publishes the snapshot and emits a SafetyEvent on CRITICAL.
        self._health = HealthMonitor()
        self.log.info(
            f"HealthMonitor armed: {len(self._health._components)} components, "
            f"recover_hysteresis={self._health.RECOVER_HYSTERESIS_S}s")

        self.spawn_thread(self._status_sub,     name="StatusSub")
        self.spawn_thread(self._imu_sub,        name="ImuSub")
        self.spawn_thread(self._loc_status_sub, name="LocSub")
        self.spawn_thread(self._rtk_sub,        name="RtkSub")
        self.spawn_thread(self._lte_sub,        name="LteSub")
        self.spawn_thread(self._lidar_freshness_sub, name="LidarFresh")
        self.spawn_thread(self._fall_detector,  name="FallDet")

    def step(self) -> None:
        # Refresh dev_override from the auth proxy. The proxy is a plain
        # dict-like — Manager.dict in prod, a stub dict in tests — so
        # .get() works either way.
        authed = bool(self._auth_proxy
                       and self._auth_proxy.get("authenticated", False))
        self._apply_auth_state(authed)

        # E1 — comm loss (placeholder: assume CommProcess updates heartbeat)
        # Snapshot under lock so the consumer threads can keep updating
        # while step() works on a consistent view.
        with self._lock:
            status = self._latest_status
            loc = self._latest_loc_status
            rtk = self._latest_rtk

        # E2 — battery (BatteryMonitor.update() runs the 3-tier ladder
        # and fires self._on_battery_action on a transition; we just feed
        # SoC and let the monitor decide).
        if status is not None and self._battery is not None:
            soc_pct = float(status.battery_soc) * 100.0
            self._battery.update(soc_pct)

        # E4 — localization degraded (RTK lost beyond tolerance OR full DR)
        if loc is not None:
            sigma_max = float(self.cfg.get("safety",
                                           "localization_sigma_max_m",
                                           default=2.0))
            degraded = (loc.source == "dead_reckoning"
                        or loc.sigma_xy > sigma_max)
            if degraded and not self._loc_degraded_emitted:
                self._emit("E4",
                           f"localization degraded: source={loc.source} "
                           f"σxy={loc.sigma_xy:.2f}m  reason={loc.fallback_reason}",
                           "slow to halt; wait for RTK recovery")
                self._loc_degraded_emitted = True
            elif not degraded and self._loc_degraded_emitted:
                # Recovered — allow re-emit on next degradation
                self._loc_degraded_emitted = False

        # E5 — fault codes
        if status and status.fault_codes:
            self._emit("E5",
                       f"fault codes: {status.fault_codes}",
                       "stop and request maintenance")

        # E6 — geofence. Only when we have a real RTK fix; on dead-reckoning
        # the lat/lon doesn't move with the robot, so fence checks would be
        # stale. The dev_override path inside Geofence.check() returns SAFE
        # immediately, so we still call it to keep edge-trigger state clean.
        if (self._geofence is not None and rtk is not None
                and rtk.fix_quality in (RTK_FIX_FIXED, RTK_FIX_FLOAT)):
            ev = self._geofence.check(rtk.lat, rtk.lon)
            self._handle_geofence_event(ev)

        # ── HealthMonitor: freshness checks + publish snapshot.
        # Per-event component reports happen inside the subscriber threads
        # (battery/rtk/lte). Freshness-based components (lidar/ext_imu)
        # are evaluated here on every tick since their "OK" signal is the
        # absence of stalls, not an event.
        if self._health is not None:
            self._report_lidar_freshness()
            self._publish_health_snapshot()
            self._handle_health_transition()

    # ─── Auth proxy plumbing ───
    def _apply_auth_state(self, authed: bool) -> None:
        """Propagate auth state to the gate modules + log transitions."""
        if self._battery is not None:
            self._battery.dev_override = authed
        if self._geofence is not None:
            self._geofence.dev_override = authed
        if authed != self._last_auth_seen:
            publish_audit(
                self.queues, category="permission",
                event="safety_dev_override",
                actor="safety",
                params={"authenticated": authed})
            self.log.info(
                f"safety dev_override → {'ON' if authed else 'OFF'}")
            self._last_auth_seen = authed

    # ─── BatteryMonitor callback ───
    def _on_battery_action(self, action: BatteryAction, soc_pct: float) -> None:
        """Translate a ladder transition into a SafetyEvent. Called from
        within BatteryMonitor.update() on the step() thread, so no extra
        locking needed."""
        if action == BatteryAction.NONE:
            return
        if action == BatteryAction.WARN:
            self._emit("E2",
                       f"battery WARN: {soc_pct:.0f}%",
                       "operator alert; mission continues")
        elif action == BatteryAction.RTH:
            self._emit("E2",
                       f"battery RTH: {soc_pct:.0f}%",
                       "abort mission; return to charger")
        elif action == BatteryAction.EMERGENCY:
            self._emit("E2",
                       f"battery EMERGENCY: {soc_pct:.0f}%",
                       "stand mode now; immediate alert")

    # ─── Geofence handling ───
    def _handle_geofence_event(self, ev) -> None:
        """Edge-trigger E6 on state transitions so we don't spam every tick."""
        if ev.state == self._geofence_state:
            return
        self._geofence_state = ev.state
        if ev.state == FenceState.APPROACHING:
            self._emit("E6",
                       f"geofence approaching: {ev.message}",
                       "steer toward fence centroid")
        elif ev.state == FenceState.VIOLATION:
            self._emit("E6",
                       f"geofence violation: {ev.message}",
                       "halt motion; force cmd_vel=(0,0)")
        # SAFE transitions are intentionally not emitted — operator-facing
        # noise that adds nothing for the audit log.

    # ─── Subscribers ───
    def _status_sub(self):
        while self.is_running():
            st = consume(self.queues.robot_status, timeout=0.1)
            if st is None:
                continue
            with self._lock:
                self._latest_status = st
            self._report_battery_health(st)
            self._report_rk_temp_health(st)

    def _imu_sub(self):
        while self.is_running():
            d = consume(self.queues.imu, timeout=0.05)
            if d is not None:
                with self._lock:
                    self._latest_imu = d

    def _loc_status_sub(self):
        while self.is_running():
            ls = consume(self.queues.localization_status, timeout=0.1)
            if ls is None:
                continue
            with self._lock:
                self._latest_loc_status = ls
            self._report_dds_health(ls)

    def _rtk_sub(self):
        while self.is_running():
            r = consume(self.queues.rtk, timeout=0.1)
            if r is None:
                continue
            with self._lock:
                self._latest_rtk = r
            self._report_rtk_health(r)

    def _lte_sub(self):
        """LTE link status → lte component health. CommProcess also reads
        this queue for failover decisions — both consumers run in their
        own processes so the queue's drop-oldest behavior preserves
        latest-state semantics for each."""
        while self.is_running():
            ls = consume(self.queues.lte_status, timeout=0.2)
            if ls is None:
                continue
            self._report_lte_health(ls)

    def _lidar_freshness_sub(self):
        """Drain lidar_ref to track last-seen timestamp.

        We don't read the SHM payload — SLAMBridge owns that pipeline. The
        ref still lands in our queue because it's published before being
        consumed by SLAM (in production both are subscribed). Here it's
        used purely as a heartbeat: stale refs mean the LiDAR adapter or
        Go2 DDS subscription has stalled.
        """
        while self.is_running():
            ref = consume(self.queues.lidar_ref, timeout=0.2)
            if ref is None:
                continue
            with self._lock:
                self._lidar_last_seen = time.monotonic()

    def _fall_detector(self):
        """E3 — high acc spike → fall suspected."""
        thresh_g = float(self.cfg.get("safety", "fall_detect_acc_g", default=2.5))
        thresh = thresh_g * 9.81
        while self.is_running():
            time.sleep(0.05)
            with self._lock:
                d = self._latest_imu
            if d is None:
                continue
            acc_norm = float(np.linalg.norm(d.linear_acc))
            # subtract gravity reading is tricky — use deviation from 9.81
            if abs(acc_norm - 9.81) > thresh:
                self._emit("E3",
                           f"abnormal accel: {acc_norm:.2f} m/s²",
                           "stop motors, await recovery")

    # ─── Health component reporters ───
    # Each method maps a domain signal to ComponentState. Thresholds are
    # configurable via the safety.* config namespace so an operator can
    # tighten or relax them per deployment without touching code.
    def _report_battery_health(self, status: RobotStatus) -> None:
        if self._health is None:
            return
        soc_pct = float(status.battery_soc) * 100.0
        emerg = self._battery.EMERGENCY_THRESHOLD if self._battery else 10.0
        warn = self._battery.WARN_THRESHOLD if self._battery else 30.0
        if soc_pct <= emerg:
            state = ComponentState.FAILED
        elif soc_pct < warn:
            state = ComponentState.DEGRADED
        else:
            state = ComponentState.OK
        self._health.report("battery", state,
                            metric={"soc_pct": soc_pct},
                            reason=f"SoC={soc_pct:.0f}%")

    def _report_rk_temp_health(self, status: RobotStatus) -> None:
        if self._health is None:
            return
        temp = float(status.motor_temp_max)
        warn_c = float(self.cfg.get("safety", "rk_temp_warn_c", default=80.0))
        fail_c = float(self.cfg.get("safety", "rk_temp_fail_c", default=90.0))
        if temp >= fail_c:
            state = ComponentState.FAILED
        elif temp >= warn_c:
            state = ComponentState.DEGRADED
        else:
            state = ComponentState.OK
        self._health.report("rk_temp", state,
                            metric={"temp_c": temp},
                            reason=f"max={temp:.1f}°C")

    def _report_rtk_health(self, rtk: RtkFix) -> None:
        if self._health is None:
            return
        if rtk.fix_quality == RTK_FIX_FIXED:
            state = ComponentState.OK
        elif rtk.fix_quality == RTK_FIX_FLOAT:
            state = ComponentState.DEGRADED
        else:
            # GPS / DGPS / NONE — RTK precision lost. Counts as FAILED
            # because the navigation precision budget assumes Fixed/Float.
            state = ComponentState.FAILED
        self._health.report("rtk", state,
                            metric={"fix_quality": rtk.fix_quality,
                                    "sigma_xy": rtk.sigma_xy},
                            reason=f"quality={rtk.fix_quality}")

    def _report_dds_health(self, loc_status) -> None:
        """DDS link health is inferred from LocalizationStatus: when the
        source falls back to dead_reckoning, either Go2's pose stream is
        gone or RTK + ext_imu are both unhealthy. The latter is already
        reported via their own components, so we attribute persistent
        dead_reckoning to DDS as a coarse proxy."""
        if self._health is None:
            return
        if loc_status.source == "dead_reckoning":
            state = ComponentState.DEGRADED
        else:
            state = ComponentState.OK
        self._health.report("dds", state,
                            metric={"source": loc_status.source},
                            reason=loc_status.fallback_reason or "")

    def _report_lte_health(self, lte_status) -> None:
        if self._health is None:
            return
        if lte_status.pdp_active:
            state = ComponentState.OK
        elif lte_status.registered == LTE_NOT_REGISTERED:
            state = ComponentState.FAILED
        else:
            state = ComponentState.DEGRADED
        self._health.report("lte", state,
                            metric={"rsrp_dbm": lte_status.rsrp_dbm,
                                    "registered": lte_status.registered,
                                    "pdp_active": int(lte_status.pdp_active)},
                            reason=f"rsrp={lte_status.rsrp_dbm:.0f}dBm")

    def _report_lidar_freshness(self) -> None:
        """Called from step() — no payload-side signal, just a watchdog
        over the lidar_ref heartbeat."""
        if self._health is None:
            return
        with self._lock:
            last_seen = self._lidar_last_seen
        if last_seen == 0.0:
            # Never seen a scan yet — boot grace period, stay OK so we
            # don't trip CRITICAL just from startup ordering.
            self._health.report("lidar", ComponentState.OK,
                                reason="awaiting first scan")
            return
        age = time.monotonic() - last_seen
        warn_s = float(self.cfg.get("safety", "lidar_stale_warn_s", default=1.0))
        fail_s = float(self.cfg.get("safety", "lidar_stale_fail_s", default=5.0))
        if age >= fail_s:
            state = ComponentState.FAILED
        elif age >= warn_s:
            state = ComponentState.DEGRADED
        else:
            state = ComponentState.OK
        self._health.report("lidar", state,
                            metric={"age_s": age},
                            reason=f"last_seen={age:.1f}s ago")

    # ─── Health publish + transition ───
    def _publish_health_snapshot(self) -> None:
        """Push the current HealthMonitor roll-up to the swarm_health bus
        for follower→hub aggregation and operator-app display."""
        if self._health is None:
            return
        robot_id = str(self.cfg.get("system", "robot_id", default="robot-000"))
        payload = self._health.to_health_message(robot_id)
        publish(self.queues.swarm_health, payload)

    def _handle_health_transition(self) -> None:
        """Audit + emit on every overall-state edge. CRITICAL is loud
        enough to also publish a SafetyEvent (E7) so Mission can react
        directly without subscribing to swarm_health."""
        if self._health is None:
            return
        cur = self._health.snapshot().overall
        if cur == self._last_health_state:
            return
        prev = self._last_health_state
        self._last_health_state = cur
        publish_audit(
            self.queues, category="safety", event="health_state_change",
            actor="safety",
            params={"from": prev.name, "to": cur.name})
        self.log.warning(f"health: {prev.name} → {cur.name}")
        if cur == HealthState.CRITICAL:
            snap = self._health.snapshot()
            failed = [n for n, c in snap.components.items()
                      if c.state == ComponentState.FAILED]
            self._emit("E7",
                       f"system health CRITICAL: failed={failed}",
                       "abort mission; halt motion")

    def _emit(self, code: str, desc: str, action: str) -> None:
        ev = SafetyEvent(
            header=Header.now(), code=code,
            description=desc, suggested_action=action,
        )
        publish(self.queues.safety_event, ev)
        self._stats["events"] += 1
        if self.log:
            self.log.warning(f"[{code}] {desc} → {action}")
