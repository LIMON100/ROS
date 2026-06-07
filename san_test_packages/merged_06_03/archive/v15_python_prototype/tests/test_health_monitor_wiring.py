"""HealthMonitor wiring in SafetyProcess.

Validates that the safety daemon actually constructs the monitor,
that each subscriber feeds its component, and that overall-state edges
publish to swarm_health + audit + emit a SafetyEvent on CRITICAL.

The unit-level HealthMonitor logic (weight buckets, hysteresis) is
covered in tests/test_health_monitor.py — these tests focus on the
integration: SafetyProcess.subscribe → HealthMonitor.report →
SafetyProcess.step → swarm_health/audit/safety_event publishes.
"""
from __future__ import annotations

import threading
import time
from unittest.mock import MagicMock

import pytest

from core import make_topic_queues
from core.ipc import consume, publish
from core.messages import (
    LTE_NOT_REGISTERED,
    LTE_REGISTERED_HOME,
    RTK_FIX_FIXED,
    RTK_FIX_FLOAT,
    RTK_FIX_NONE,
    Header,
    LocalizationStatus,
    LteStatus,
    RobotStatus,
    RtkFix,
)
from safety.health_monitor import (
    ComponentState,
    HealthMonitor,
    HealthState,
)
from safety.safety_process import SafetyProcess


# ─── Fixtures ──────────────────────────────────────────────────────────
@pytest.fixture
def auth_proxy():
    return {"authenticated": False, "ts_mono": 0.0, "reason": "test"}


@pytest.fixture
def fake_cfg():
    cfg = MagicMock()
    overrides: dict = {}

    def _get(*keys, default=None):
        return overrides.get(tuple(keys), default)
    cfg.get.side_effect = _get
    cfg._overrides = overrides
    return cfg


def _make_safety(auth_proxy, fake_cfg) -> SafetyProcess:
    p = SafetyProcess.__new__(SafetyProcess)
    p.queues = make_topic_queues()
    p.cfg = fake_cfg
    p._auth_proxy = auth_proxy
    p.log = MagicMock()
    p._lock = threading.Lock()
    p._latest_status = None
    p._latest_imu = None
    p._latest_loc_status = None
    p._latest_rtk = None
    p._lidar_last_seen = 0.0
    p._last_heartbeat = 0.0
    p._loc_degraded_emitted = False
    from safety.geofence import FenceState
    p._geofence_state = FenceState.SAFE
    p._last_auth_seen = False
    p._last_health_state = HealthState.NORMAL
    p._battery = None
    p._geofence = None
    p._health = None
    p._stats = {"events": 0}
    p.spawn_thread = lambda target, name: None
    p.is_running = lambda: False
    return p


# ─── Setup / instantiation ─────────────────────────────────────────────
def test_health_monitor_constructed_in_setup(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    assert isinstance(p._health, HealthMonitor)


def test_setup_logs_component_count_and_hysteresis(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    # _health is built in setup(); log was captured by MagicMock
    info_calls = [str(c) for c in p.log.info.call_args_list]
    assert any("HealthMonitor armed" in s for s in info_calls), info_calls


# ─── Per-component reporting via the subscriber path ──────────────────
def test_battery_status_degrades_battery_component(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    # SoC 25% — between WARN(30) and RTH(20). Component state = DEGRADED.
    p._report_battery_health(RobotStatus(
        header=Header.now(), battery_soc=0.25))
    snap = p._health.snapshot()
    assert snap.components["battery"].state == ComponentState.DEGRADED
    # SoC 5% — below EMERGENCY. FAILED.
    p._report_battery_health(RobotStatus(
        header=Header.now(), battery_soc=0.05))
    assert p._health.snapshot().components["battery"].state \
        == ComponentState.FAILED


def test_rtk_quality_maps_to_health_component(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    p._report_rtk_health(RtkFix(
        header=Header.now(), fix_quality=RTK_FIX_FIXED))
    assert p._health.snapshot().components["rtk"].state == ComponentState.OK
    p._report_rtk_health(RtkFix(
        header=Header.now(), fix_quality=RTK_FIX_FLOAT))
    assert p._health.snapshot().components["rtk"].state \
        == ComponentState.DEGRADED
    p._report_rtk_health(RtkFix(
        header=Header.now(), fix_quality=RTK_FIX_NONE))
    assert p._health.snapshot().components["rtk"].state \
        == ComponentState.FAILED


def test_lte_pdp_active_is_ok_otherwise_degraded(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    p._report_lte_health(LteStatus(
        header=Header.now(), registered=LTE_REGISTERED_HOME, pdp_active=True))
    assert p._health.snapshot().components["lte"].state == ComponentState.OK
    p._report_lte_health(LteStatus(
        header=Header.now(), registered=LTE_REGISTERED_HOME, pdp_active=False))
    assert p._health.snapshot().components["lte"].state \
        == ComponentState.DEGRADED
    p._report_lte_health(LteStatus(
        header=Header.now(), registered=LTE_NOT_REGISTERED, pdp_active=False))
    assert p._health.snapshot().components["lte"].state \
        == ComponentState.FAILED


def test_dds_dead_reckoning_degrades_component(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    p._report_dds_health(LocalizationStatus(
        header=Header.now(), source="dead_reckoning",
        sigma_xy=5.0, fallback_reason="rtk_lost"))
    assert p._health.snapshot().components["dds"].state \
        == ComponentState.DEGRADED
    p._report_dds_health(LocalizationStatus(
        header=Header.now(), source="rtk_imu",
        sigma_xy=0.05, fallback_reason=""))
    assert p._health.snapshot().components["dds"].state == ComponentState.OK


def test_lidar_freshness_stale_marks_degraded_then_failed(auth_proxy, fake_cfg):
    fake_cfg._overrides[("safety", "lidar_stale_warn_s")] = 0.1
    fake_cfg._overrides[("safety", "lidar_stale_fail_s")] = 0.3
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()

    # Just-seen → OK
    p._lidar_last_seen = time.monotonic()
    p._report_lidar_freshness()
    assert p._health.snapshot().components["lidar"].state == ComponentState.OK

    # Force the freshness boundary by clock-back
    p._lidar_last_seen = time.monotonic() - 0.15
    p._report_lidar_freshness()
    assert p._health.snapshot().components["lidar"].state \
        == ComponentState.DEGRADED

    p._lidar_last_seen = time.monotonic() - 1.0
    p._report_lidar_freshness()
    assert p._health.snapshot().components["lidar"].state \
        == ComponentState.FAILED


def test_lidar_never_seen_stays_ok_during_boot(auth_proxy, fake_cfg):
    """Startup ordering safety: if LidarFresh hasn't fired yet, don't
    report FAILED — that would push the system into CRITICAL on boot."""
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    assert p._lidar_last_seen == 0.0
    p._report_lidar_freshness()
    assert p._health.snapshot().components["lidar"].state == ComponentState.OK


# ─── Snapshot publish + transition handling ───────────────────────────
def test_step_publishes_swarm_health_snapshot(auth_proxy, fake_cfg):
    fake_cfg._overrides[("system", "robot_id")] = "robot-042"
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    p.step()       # one tick → one publish
    msg = consume(p.queues.swarm_health, timeout=0.5)
    assert msg is not None
    assert msg["robot_id"] == "robot-042"
    assert msg["overall"] == "NORMAL"
    assert "battery" in msg["components"]
    assert msg["components"]["battery"]["status"] == "OK"


def test_normal_to_critical_emits_safety_event_and_audit(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()

    # Stack failures to push past CRITICAL_WEIGHT_SUM=7:
    #   battery FAILED  → 3 * 2 = 6
    #   lidar FAILED    → 3 * 2 = 6  (total 12) → CRITICAL
    p._report_battery_health(RobotStatus(
        header=Header.now(), battery_soc=0.02))
    p._lidar_last_seen = time.monotonic() - 10.0
    fake_cfg._overrides[("safety", "lidar_stale_fail_s")] = 1.0
    p._report_lidar_freshness()

    assert p._health.snapshot().overall == HealthState.CRITICAL

    # Trigger the transition handler
    p._handle_health_transition()

    # SafetyEvent E7 emitted
    se = consume(p.queues.safety_event, timeout=0.3)
    assert se is not None
    assert se.code == "E7"
    assert "CRITICAL" in se.description

    # Audit event emitted on transition
    events = []
    while True:
        ev = consume(p.queues.audit_event, timeout=0.05)
        if ev is None:
            break
        events.append(ev)
    assert any(e["event"] == "health_state_change"
               and e["params"]["to"] == "CRITICAL"
               for e in events), events


def test_transition_handler_idempotent_when_state_unchanged(
        auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    # First call sets _last_health_state to current (NORMAL → NORMAL = no edge)
    p._handle_health_transition()
    p._handle_health_transition()
    # No audit / no safety event
    assert consume(p.queues.audit_event, timeout=0.05) is None
    assert consume(p.queues.safety_event, timeout=0.05) is None


# ─── End-to-end via the public subscriber path ────────────────────────
def test_status_queue_publish_drives_battery_component(auth_proxy, fake_cfg):
    """Push a RobotStatus through the actual queue+subscriber, simulate
    one drain cycle, and assert HealthMonitor reflects it. Smoke test of
    the queue → subscriber → report() path."""
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    publish(p.queues.robot_status, RobotStatus(
        header=Header.now(), battery_soc=0.05))
    # Drain one message synchronously, mimicking the StatusSub thread
    st = consume(p.queues.robot_status, timeout=0.5)
    assert st is not None
    p._report_battery_health(st)
    assert p._health.snapshot().components["battery"].state \
        == ComponentState.FAILED
