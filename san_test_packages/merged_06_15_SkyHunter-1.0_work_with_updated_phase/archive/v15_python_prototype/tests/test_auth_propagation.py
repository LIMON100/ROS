"""End-to-end auth propagation: BLE → queue → main relay → proxy → consumers.

Validates the consumer-side wiring of the auth_state pipeline. The chain:

  BleControlProcess (publishes auth_state queue events)
    → main._auth_state_relay (drains queue, mutates Manager.dict proxy)
    → SafetyProcess.step() (reads proxy, sets BatteryMonitor + Geofence
      dev_override)
    → MissionProcess.step() (reads proxy, calls
      OperationalModeController.set_pin_authenticated)

These tests use plain dicts in place of Manager.dict — every consumer
only calls .get("authenticated", False), so a dict suffices and we
avoid spinning up an mp.Manager per test (~50ms each).
"""
from __future__ import annotations

import threading
import time
from unittest.mock import MagicMock

import pytest

from core import make_topic_queues
from core.messages import (
    RTK_FIX_FIXED,
    Header,
    RobotStatus,
    RtkFix,
)
from mission.mission_process import MissionContext, MissionProcess
from mission.operational_modes import OperationalMode
from safety.geofence import FenceState
from safety.safety_process import SafetyProcess


# ─── Fixtures ──────────────────────────────────────────────────────────
@pytest.fixture
def auth_proxy():
    """Plain dict as a Manager.dict stand-in. Consumers only .get()."""
    return {"authenticated": False, "ts_mono": 0.0, "reason": "test"}


@pytest.fixture
def fake_cfg():
    cfg = MagicMock()
    overrides: dict = {}

    def _get(*keys, default=None):
        return overrides.get(tuple(keys), default)
    cfg.get.side_effect = _get
    cfg._overrides = overrides     # tests can poke values in
    return cfg


def _make_safety(auth_proxy, fake_cfg) -> SafetyProcess:
    """Build a SafetyProcess instance without spawning the BaseProcess
    machinery. Just enough to drive setup() and step() directly."""
    from safety.health_monitor import HealthState
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
    p._geofence_state = FenceState.SAFE
    p._last_auth_seen = False
    p._last_health_state = HealthState.NORMAL
    p._battery = None
    p._geofence = None
    p._health = None
    p._stats = {"events": 0}
    p.spawn_thread = lambda target, name: None     # don't actually spawn
    p.is_running = lambda: False
    return p


def _make_mission(auth_proxy, fake_cfg) -> MissionProcess:
    p = MissionProcess.__new__(MissionProcess)
    p.queues = make_topic_queues()
    p.cfg = fake_cfg
    p._auth_proxy = auth_proxy
    p.log = MagicMock()
    p._lock = threading.Lock()
    p.planner = MagicMock()
    p.planner.due_route.return_value = None        # no schedule pressure
    p.tree = None
    p.ctx = MissionContext()
    p.rollback = None
    p._last_auth_seen = False
    p.spawn_thread = lambda target, name: None
    p.is_running = lambda: False
    return p


# ─── SafetyProcess: BatteryMonitor wired in ────────────────────────────
def test_safety_battery_monitor_replaces_inline_check(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    assert p._battery is not None, "BatteryMonitor must be built in setup()"

    # SoC under EMERGENCY threshold → emit a SafetyEvent
    p._latest_status = RobotStatus(
        header=Header.now(), battery_soc=0.05)
    p.step()
    from core.ipc import consume
    ev = consume(p.queues.safety_event, timeout=0.5)
    assert ev is not None
    assert ev.code == "E2"
    assert "EMERGENCY" in ev.description


def test_safety_battery_dev_override_propagated_from_auth_proxy(
        auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()

    # Authenticate via the proxy; battery falls below EMERGENCY but
    # BatteryMonitor.dev_override should suppress the SafetyEvent.
    auth_proxy["authenticated"] = True
    p._latest_status = RobotStatus(
        header=Header.now(), battery_soc=0.05)
    p.step()

    from core.ipc import consume
    assert consume(p.queues.safety_event, timeout=0.3) is None, (
        "dev_override=True should suppress E2 emission")
    assert p._battery.dev_override is True


def test_safety_battery_auth_toggle_audits_transition(auth_proxy, fake_cfg):
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()

    p.step()                            # auth=False, no transition (matches init)
    auth_proxy["authenticated"] = True
    p.step()                            # auth=True, edge → audit + log

    from core.ipc import consume
    events: list = []
    while True:
        ev = consume(p.queues.audit_event, timeout=0.05)
        if ev is None:
            break
        events.append(ev)
    assert any(e["event"] == "safety_dev_override"
               and e["params"]["authenticated"] is True
               for e in events), events


# ─── SafetyProcess: Geofence ───────────────────────────────────────────
def test_safety_geofence_emits_violation_when_outside_polygon(
        auth_proxy, fake_cfg):
    # Square fence around (37.5, 127.0)
    fake_cfg._overrides[("safety", "geofence_polygon")] = [
        (37.499, 126.999), (37.501, 126.999),
        (37.501, 127.001), (37.499, 127.001),
    ]
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    assert p._geofence is not None

    # Pose far outside the polygon
    p._latest_rtk = RtkFix(
        header=Header.now(),
        lat=37.6, lon=127.1,
        fix_quality=RTK_FIX_FIXED,
        sigma_xy=0.02,
    )
    p.step()
    from core.ipc import consume
    # Drain — geofence emits E6 on the SAFE→VIOLATION edge
    seen = []
    while True:
        ev = consume(p.queues.safety_event, timeout=0.05)
        if ev is None:
            break
        seen.append(ev)
    assert any(e.code == "E6" and "violation" in e.description
               for e in seen), seen


def test_safety_geofence_skipped_without_rtk_fix(auth_proxy, fake_cfg):
    fake_cfg._overrides[("safety", "geofence_polygon")] = [
        (37.499, 126.999), (37.501, 126.999),
        (37.501, 127.001), (37.499, 127.001),
    ]
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()

    # No RtkFix received — geofence block should be skipped entirely.
    p.step()
    from core.ipc import consume
    assert consume(p.queues.safety_event, timeout=0.2) is None


def test_safety_geofence_disabled_when_polygon_invalid(auth_proxy, fake_cfg):
    fake_cfg._overrides[("safety", "geofence_polygon")] = [
        (37.5, 127.0), (37.51, 127.0),     # only 2 points
    ]
    p = _make_safety(auth_proxy, fake_cfg)
    p.setup()
    assert p._geofence is None, (
        "Polygon with <3 vertices should disable geofence, not crash setup")


# ─── MissionProcess: OperationalModeController wired in ────────────────
def test_mission_default_mode_is_recon(auth_proxy, fake_cfg):
    p = _make_mission(auth_proxy, fake_cfg)
    p.setup()
    assert p.ctx.mode_controller is not None
    assert p.ctx.mode_controller.current == OperationalMode.RECON


def test_mission_request_dev_test_blocked_without_pin(auth_proxy, fake_cfg):
    p = _make_mission(auth_proxy, fake_cfg)
    p.setup()

    # No auth → request_mode(DEV_TEST) refuses
    ok, msg = p.ctx.mode_controller.request_mode(OperationalMode.DEV_TEST)
    assert ok is False
    assert "PIN" in msg


def test_mission_request_dev_test_allowed_after_auth_propagation(
        auth_proxy, fake_cfg):
    p = _make_mission(auth_proxy, fake_cfg)
    p.setup()

    auth_proxy["authenticated"] = True
    p.step()           # propagates auth_proxy → controller

    ok, msg = p.ctx.mode_controller.request_mode(OperationalMode.DEV_TEST)
    assert ok is True, msg
    assert p.ctx.mode_controller.current == OperationalMode.DEV_TEST


def test_mission_pin_authenticated_mirrored_on_ctx(auth_proxy, fake_cfg):
    p = _make_mission(auth_proxy, fake_cfg)
    p.setup()

    assert p.ctx.pin_authenticated is False
    auth_proxy["authenticated"] = True
    p.step()
    assert p.ctx.pin_authenticated is True


# ─── End-to-end: queue → relay → proxy → consumer ──────────────────────
def test_relay_pumps_queue_event_into_proxy():
    """The relay thread in main.py drains the queue and mutates the proxy.
    This test imports the helper directly and runs it against a stub
    proxy so we don't have to instantiate Manager()."""
    from main import _auth_state_relay
    queues = make_topic_queues()
    proxy: dict = {"authenticated": False, "ts_mono": 0.0, "reason": "init"}
    stop = threading.Event()
    t = threading.Thread(
        target=_auth_state_relay,
        args=(queues, proxy, stop),
        daemon=True,
    )
    t.start()

    from core.ipc import publish
    publish(queues.auth_state, {
        "authenticated": True,
        "ts_mono": 12345.0,
        "reason": "pin_verified",
    })

    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        if proxy["authenticated"]:
            break
        time.sleep(0.02)
    stop.set()
    t.join(timeout=0.5)

    assert proxy["authenticated"] is True
    assert proxy["reason"] == "pin_verified"
    assert proxy["ts_mono"] == 12345.0
