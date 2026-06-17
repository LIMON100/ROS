"""Health monitoring + degraded-mode FSM (SDD Rev.A.6 §9.6).

Ten components track their own state; HealthMonitor rolls them up
into one of four overall states. The point is to keep the mission
running when a single non-critical sensor degrades, instead of
forcing a full abort on the first hiccup.

Components & severity weights:
  rtk:      2    LiDAR:    3    battery:  3
  dds:      2    ext_imu:  2    wifi6:    2
  imx678:   1    thermal:  1    lte:      1    rk_temp:  1

A FAILED component contributes weight × 2; DEGRADED contributes
weight × 1. The cumulative score buckets the overall state:

  total ≥ 7 → CRITICAL    (mission abort)
  total ≥ 4 → DEGRADED_L2 (operator alert + reduced speed)
  total ≥ 1 → DEGRADED_L1 (logged, continue)
  total = 0 → NORMAL

Hysteresis: degrade is immediate; recovery requires 30 s with the
component back to OK (prevents rapid flap from a marginal sensor).
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Dict, Optional


class HealthState(IntEnum):
    NORMAL = 0
    DEGRADED_L1 = 1     # at least one minor sensor wobble
    DEGRADED_L2 = 2     # multiple sensors or one risky one
    CRITICAL = 3         # mission unsafe


class ComponentState(IntEnum):
    OK = 0
    DEGRADED = 1
    FAILED = 2


_SEVERITY_WEIGHT = {
    "rtk":      2,
    "lidar":    3,
    "battery":  3,
    "dds":      2,
    "ext_imu":  2,
    "wifi6":    2,
    "imx678":   1,
    "thermal":  1,
    "lte":      1,
    "rk_temp":  1,
}


@dataclass
class ComponentHealth:
    name: str
    state: ComponentState = ComponentState.OK
    last_update_ts: float = 0.0
    failure_count: int = 0
    metric: Dict[str, float] = field(default_factory=dict)
    reason: str = ""

    @property
    def severity_weight(self) -> int:
        return _SEVERITY_WEIGHT.get(self.name.lower(), 1)


@dataclass
class HealthSnapshot:
    overall: HealthState
    components: Dict[str, ComponentHealth]
    ts: float


class HealthMonitor:
    """Track component health and compute overall state."""

    DEGRADE_HYSTERESIS_S = 0.0       # immediate degrade
    RECOVER_HYSTERESIS_S = 30.0      # 30 s stable to recover

    L1_WEIGHT_SUM = 1
    L2_WEIGHT_SUM = 4
    CRITICAL_WEIGHT_SUM = 7

    _DEFAULT_COMPONENTS = (
        "rtk", "lidar", "imx678", "thermal", "ext_imu",
        "lte", "wifi6", "dds", "battery", "rk_temp",
    )

    def __init__(self):
        self._components: Dict[str, ComponentHealth] = {
            n: ComponentHealth(name=n) for n in self._DEFAULT_COMPONENTS
        }
        self._overall: HealthState = HealthState.NORMAL
        self._first_clear_ts: Optional[float] = None
        # report() is called from any DDS subscriber thread (per-component
        # heartbeats fan in concurrently). snapshot() / _recompute_overall()
        # iterate the dict, so an unlocked report() that adds a new
        # component mid-iteration raises RuntimeError.
        self._lock = threading.Lock()

    # ─── Ingest ───
    def report(self,
               name: str,
               state: ComponentState,
               metric: Optional[Dict[str, float]] = None,
               reason: str = "") -> None:
        with self._lock:
            c = self._components.get(name)
            if c is None:
                c = ComponentHealth(name=name)
                self._components[name] = c
            prev = c.state
            c.state = state
            c.last_update_ts = time.monotonic()
            c.metric = metric or {}
            c.reason = reason
            if state != ComponentState.OK and prev == ComponentState.OK:
                c.failure_count += 1
            self._recompute_overall()

    # ─── Output ───
    def snapshot(self) -> HealthSnapshot:
        with self._lock:
            return HealthSnapshot(
                overall=self._overall,
                components=dict(self._components),
                ts=time.monotonic(),
            )

    def to_health_message(self, robot_id: str) -> Dict:
        """1 Hz publish payload for the sw/health DDS topic."""
        snap = self.snapshot()
        return {
            "robot_id": robot_id,
            "ts_ms":    int(time.time() * 1000),
            "overall":  snap.overall.name,
            "components": {
                name: {
                    "status": c.state.name,
                    "metric": c.metric,
                    "reason": c.reason,
                }
                for name, c in snap.components.items()
            },
        }

    # ─── Internal aggregation ───
    def _recompute_overall(self) -> None:
        sum_degraded = sum(
            c.severity_weight for c in self._components.values()
            if c.state == ComponentState.DEGRADED)
        sum_failed = sum(
            c.severity_weight * 2 for c in self._components.values()
            if c.state == ComponentState.FAILED)
        total = sum_degraded + sum_failed
        new_state = self._weight_to_state(total)

        if new_state > self._overall:
            self._overall = new_state              # degrade immediately
            self._first_clear_ts = None
            return

        if new_state < self._overall:
            now = time.monotonic()
            if self._first_clear_ts is None:
                self._first_clear_ts = now
            elif (now - self._first_clear_ts) >= self.RECOVER_HYSTERESIS_S:
                self._overall = new_state
                self._first_clear_ts = None
            return

        # same level — clear any in-flight recovery timer
        self._first_clear_ts = None

    @classmethod
    def _weight_to_state(cls, total: int) -> HealthState:
        if total >= cls.CRITICAL_WEIGHT_SUM:
            return HealthState.CRITICAL
        if total >= cls.L2_WEIGHT_SUM:
            return HealthState.DEGRADED_L2
        if total >= cls.L1_WEIGHT_SUM:
            return HealthState.DEGRADED_L1
        return HealthState.NORMAL
