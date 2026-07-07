"""Tests for health monitoring + degraded mode FSM (P1-17, SDD §9.6)."""
from __future__ import annotations

import time

from safety.health_monitor import (
    ComponentState,
    HealthMonitor,
    HealthState,
)


# ─── Initial state ───
def test_initial_state_is_normal():
    h = HealthMonitor()
    assert h.snapshot().overall == HealthState.NORMAL


def test_all_10_components_initialized():
    h = HealthMonitor()
    snap = h.snapshot()
    expected = {"rtk", "lidar", "imx678", "thermal", "ext_imu",
                "lte", "wifi6", "dds", "battery", "rk_temp"}
    assert set(snap.components.keys()) == expected


# ─── Aggregation ───
def test_single_minor_component_degrade_to_l1():
    """One DEGRADED imx678 (weight 1) → L1."""
    h = HealthMonitor()
    h.report("imx678", ComponentState.DEGRADED, reason="exposure_stuck")
    assert h.snapshot().overall == HealthState.DEGRADED_L1


def test_critical_component_failure_to_l2():
    """One FAILED battery (weight 3 × 2 = 6) → L2."""
    h = HealthMonitor()
    h.report("battery", ComponentState.FAILED, reason="voltage_low")
    assert h.snapshot().overall in (HealthState.DEGRADED_L2,
                                    HealthState.CRITICAL)


def test_multiple_failures_critical():
    """Three FAILED critical components → CRITICAL."""
    h = HealthMonitor()
    h.report("battery", ComponentState.FAILED)
    h.report("lidar", ComponentState.FAILED)
    h.report("rtk", ComponentState.FAILED)
    assert h.snapshot().overall == HealthState.CRITICAL


# ─── Hysteresis ───
def test_degrade_is_immediate():
    h = HealthMonitor()
    assert h.snapshot().overall == HealthState.NORMAL
    h.report("rtk", ComponentState.FAILED)
    assert h.snapshot().overall in (HealthState.DEGRADED_L2,
                                    HealthState.CRITICAL)


def test_recovery_requires_30s_stable():
    """Degraded → Normal needs RECOVER_HYSTERESIS_S elapsed."""
    h = HealthMonitor()
    h.report("imx678", ComponentState.DEGRADED)
    assert h.snapshot().overall == HealthState.DEGRADED_L1
    h.report("imx678", ComponentState.OK)
    assert h.snapshot().overall == HealthState.DEGRADED_L1     # held
    h._first_clear_ts = time.monotonic() - 35.0
    h._recompute_overall()
    assert h.snapshot().overall == HealthState.NORMAL


def test_failure_count_increments():
    h = HealthMonitor()
    h.report("rtk", ComponentState.FAILED)
    assert h._components["rtk"].failure_count == 1
    h.report("rtk", ComponentState.OK)
    h.report("rtk", ComponentState.FAILED)
    assert h._components["rtk"].failure_count == 2


# ─── Output format ───
def test_health_message_format():
    h = HealthMonitor()
    h.report("rtk", ComponentState.OK, metric={"age_s": 0.4})
    h.report("imx678", ComponentState.DEGRADED, reason="exposure_stuck")
    msg = h.to_health_message("robot-003")
    assert msg["robot_id"] == "robot-003"
    assert "ts_ms" in msg
    assert msg["overall"] in ("NORMAL", "DEGRADED_L1",
                              "DEGRADED_L2", "CRITICAL")
    assert msg["components"]["rtk"]["status"] == "OK"
    assert msg["components"]["imx678"]["status"] == "DEGRADED"
    assert msg["components"]["imx678"]["reason"] == "exposure_stuck"


def test_metric_passed_through():
    h = HealthMonitor()
    h.report("battery", ComponentState.OK,
             metric={"soc_pct": 78.5, "voltage": 25.4})
    msg = h.to_health_message("robot-1")
    assert msg["components"]["battery"]["metric"]["soc_pct"] == 78.5


def test_unknown_component_added_dynamically():
    h = HealthMonitor()
    h.report("custom_sensor", ComponentState.DEGRADED)
    snap = h.snapshot()
    assert "custom_sensor" in snap.components


# ─── Thread safety (H8 fix) ───
def test_concurrent_reports_do_not_crash():
    """Concurrent report() calls that add brand-new components must not
    race the snapshot/recompute iteration that's reading the same dict.
    Without the lock this raises RuntimeError on at least one thread."""
    import threading

    h = HealthMonitor()
    crashes: list = []

    def reporter(prefix: str, n: int):
        try:
            for i in range(n):
                h.report(f"{prefix}_{i}", ComponentState.DEGRADED)
        except Exception as e:           # noqa: BLE001
            crashes.append(e)

    def reader(n: int):
        try:
            for _ in range(n):
                _ = h.snapshot()
                _ = h.to_health_message("robot-x")
        except Exception as e:           # noqa: BLE001
            crashes.append(e)

    threads = [
        threading.Thread(target=reporter, args=(f"thread{t}", 50))
        for t in range(4)
    ] + [
        threading.Thread(target=reader, args=(200,)) for _ in range(2)
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert crashes == [], f"races caught: {crashes!r}"
