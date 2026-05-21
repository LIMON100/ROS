"""
End-to-end FSM-driving tests for OrchestratorProcess.

We don't spawn the full multiprocessing.Process — these tests exercise
the orchestrator as an in-process object with real queues. The flow is:
  1. push a BLE command onto ble_command queue
  2. let the consumer thread pick it up
  3. assert FSM transitioned + downstream queues received the right
     side-effect messages
"""
from __future__ import annotations

import multiprocessing as mp
import queue as thread_queue
import threading
import time
from types import SimpleNamespace
from unittest.mock import MagicMock

import pytest

from control.orchestrator_process import OrchestratorProcess
from control.state_machine import ErrorCode, Opcode, Phase
from core.ipc import consume, publish


def _lightweight_queues():
    """Build only the queues the orchestrator touches, using thread-safe
    queue.Queue (no fd consumption). Avoids exhausting the fd limit
    when many tests run in sequence."""
    def Q():
        return thread_queue.Queue(maxsize=20)
    return SimpleNamespace(
        ble_command=Q(), ble_settings=Q(),
        ble_phase=Q(), ble_creds=Q(), ble_errors=Q(),
        wifi_request=Q(), wifi_progress=Q(),
        stream_request=Q(), stream_status=Q(),
    )


def _make_orch():
    cfg = MagicMock()
    cfg.get.side_effect = lambda *keys, default=None: default

    proc = OrchestratorProcess.__new__(OrchestratorProcess)
    proc.queues = _lightweight_queues()
    proc.shutdown_event = mp.Event()
    proc.cfg = cfg
    proc.log = MagicMock()
    proc._fsm = None
    proc._latest_creds = {}
    proc._budget_deadline_at = None
    proc._stats = {"ble_cmds": 0, "wifi_ups": 0, "wifi_downs": 0,
                    "stream_starts": 0, "stream_stops": 0,
                    "fsm_errors": 0, "budget_timeouts": 0}
    proc._threads = []
    proc.spawn_thread = lambda target, name: threading.Thread(
        target=target, name=name, daemon=True).start() or None
    proc.is_running = lambda: not proc.shutdown_event.is_set()
    return proc


@pytest.fixture
def orch():
    p = _make_orch()
    p.setup()
    time.sleep(0.05)
    yield p
    p.shutdown_event.set()
    time.sleep(0.1)


# ════════════════════════════════════════════════════════════════
# Initial state
# ════════════════════════════════════════════════════════════════
def test_starts_in_ble_adv(orch):
    """After setup() the FSM advanced from BOOT to BLE_ADV."""
    assert orch._fsm.phase == Phase.BLE_ADV


def test_phase_change_publishes_to_ble_queue(orch):
    """The FSM listener pushes phase ints onto ble_phase queue."""
    # Initial transition (BOOT → BLE_ADV) already happened
    msg = consume(orch.queues.ble_phase, timeout=1.0)
    assert msg == int(Phase.BLE_ADV)


# ════════════════════════════════════════════════════════════════
# WIFI_ON happy path
# ════════════════════════════════════════════════════════════════
def test_wifi_on_advances_fsm_and_emits_request(orch):
    # First any-CMD upgrades BLE_ADV → BLE_CONN, then WIFI_ON does the rest
    publish(orch.queues.ble_command, {"opcode": int(Opcode.WIFI_ON)})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.WIFI_BRINGUP
    # Side effect: wifi_request emitted
    req = consume(orch.queues.wifi_request, timeout=1.0)
    assert req == {"action": "up"}


def test_wifi_progress_100_advances_to_wifi_ready_then_starts_stream(orch):
    publish(orch.queues.ble_command, {"opcode": int(Opcode.WIFI_ON)})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.WIFI_BRINGUP
    # Drain the wifi_request that was just emitted
    consume(orch.queues.wifi_request, timeout=1.0)

    publish(orch.queues.wifi_progress, {"pct": 100, "phase": "ready"})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.WIFI_READY
    # Side effect: stream_request emitted
    sreq = consume(orch.queues.stream_request, timeout=1.0)
    assert sreq["action"] == "start"


def test_stream_playing_advances_to_streaming(orch):
    # Get to WIFI_READY first
    orch._fsm.request(Phase.BLE_CONN, reason="test")
    orch._fsm.request(Phase.WIFI_BRINGUP, reason="test")
    orch._fsm.request(Phase.WIFI_READY, reason="test")

    publish(orch.queues.stream_status, {"playing": True, "uptime_s": 2.0})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.STREAMING


# ════════════════════════════════════════════════════════════════
# Tear-down paths
# ════════════════════════════════════════════════════════════════
def test_wifi_off_tears_down_from_streaming(orch):
    # Drive to STREAMING
    orch._fsm.request(Phase.BLE_CONN, reason="t")
    orch._fsm.request(Phase.WIFI_BRINGUP, reason="t")
    orch._fsm.request(Phase.WIFI_READY, reason="t")
    orch._fsm.request(Phase.STREAMING, reason="t")
    while consume(orch.queues.stream_request, timeout=0.05):
        pass
    while consume(orch.queues.wifi_request,   timeout=0.05):
        pass

    publish(orch.queues.ble_command, {"opcode": int(Opcode.WIFI_OFF)})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.BLE_ADV
    # Side effects: stop the stream + wifi
    sr = consume(orch.queues.stream_request, timeout=1.0)
    assert sr == {"action": "stop"}
    wr = consume(orch.queues.wifi_request, timeout=1.0)
    assert wr == {"action": "down"}


def test_internal_disconnect_resets_to_ble_adv(orch):
    orch._fsm.request(Phase.BLE_CONN, reason="t")
    orch._fsm.request(Phase.WIFI_BRINGUP, reason="t")
    while consume(orch.queues.wifi_request, timeout=0.05):
        pass

    publish(orch.queues.ble_command,
             {"opcode": "_internal_disconnect", "reason": "eof"})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.BLE_ADV


def test_stream_unexpected_failure_goes_to_error(orch):
    orch._fsm.request(Phase.BLE_CONN, reason="t")
    orch._fsm.request(Phase.WIFI_BRINGUP, reason="t")
    orch._fsm.request(Phase.WIFI_READY, reason="t")
    orch._fsm.request(Phase.STREAMING, reason="t")

    publish(orch.queues.stream_status, {
        "playing": False, "error_code": int(ErrorCode.SRT_HANDSHAKE),
    })
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.ERROR
    # Error notification pushed back to BLE
    err = consume(orch.queues.ble_errors, timeout=1.0)
    assert err == int(ErrorCode.SRT_HANDSHAKE)


# ════════════════════════════════════════════════════════════════
# Illegal commands are ignored gracefully
# ════════════════════════════════════════════════════════════════
def test_wifi_off_ignored_in_ble_adv(orch):
    # WIFI_OFF makes no sense from BLE_ADV (wifi never came up)
    initial = orch._fsm.phase
    publish(orch.queues.ble_command, {"opcode": int(Opcode.WIFI_OFF)})
    time.sleep(0.3)
    # Phase unchanged (or advanced via the first-cmd upgrade to BLE_CONN —
    # but never to TEARDOWN/BLE_ADV from a WIFI_OFF)
    assert orch._fsm.phase in (initial, Phase.BLE_CONN)


def test_unknown_opcode_does_not_crash(orch):
    publish(orch.queues.ble_command, {"opcode": 0xFF})
    time.sleep(0.2)
    # Still alive, no exception leaked
    assert orch._fsm.phase != Phase.ERROR


def test_reset_clears_error_state(orch):
    # Force into ERROR
    orch._fsm.to_error(ErrorCode.STREAM_FAIL, reason="test")
    assert orch._fsm.phase == Phase.ERROR

    publish(orch.queues.ble_command, {"opcode": int(Opcode.RESET)})
    time.sleep(0.3)
    assert orch._fsm.phase == Phase.BLE_ADV


# ════════════════════════════════════════════════════════════════
# Budget watchdog
# ════════════════════════════════════════════════════════════════
def test_budget_timeout_in_wifi_bringup_goes_to_error(orch, monkeypatch):
    """If wifi never reports 100%, the budget elapses and we error out."""
    monkeypatch.setattr(orch, "WIFI_BRINGUP_BUDGET_S", 0.2)
    publish(orch.queues.ble_command, {"opcode": int(Opcode.WIFI_ON)})
    time.sleep(0.4)
    assert orch._fsm.phase == Phase.WIFI_BRINGUP
    # No progress arrives — wait for budget + step() to fire
    time.sleep(0.7)
    orch.step()    # manually invoke the tick that checks the deadline
    assert orch._fsm.phase == Phase.ERROR
    assert orch._stats["budget_timeouts"] == 1
