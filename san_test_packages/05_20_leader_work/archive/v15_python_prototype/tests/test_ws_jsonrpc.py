"""Tests for WS JSON-RPC API (P1-10, SDD Rev.A.6 §15.3).

The full WS round-trip is exercised by the existing tests/test_ws_telemetry.py
harness. Here we drive the per-method handler dispatch directly so failures
point straight at the affected handler instead of the WS plumbing.
"""
from __future__ import annotations

import asyncio
import threading
from unittest.mock import MagicMock

import pytest

from control.ws_telemetry_process import WsTelemetryProcess


def _bare_proc() -> WsTelemetryProcess:
    """Construct without invoking BaseProcess.__init__.

    `_forward_to_app_rpc` is replaced with a plain async capture so handler
    tests don't have to wait on the run_in_executor thread (which can race
    a freshly-closed event loop on Windows). The captured payloads live on
    `p.captured` for assertions.
    """
    p = WsTelemetryProcess.__new__(WsTelemetryProcess)
    p.queues = MagicMock()
    p.cfg = MagicMock()
    p.log = MagicMock()
    p._clients = set()
    p._clients_lock = asyncio.Lock()
    p._snapshot_lock = threading.Lock()
    p._event_lock = threading.Lock()
    p._event_buf = []
    p._snapshot = {}
    p._stats = {
        "rpc_received": 0, "rpc_errors": 0,
        "broadcasts": 0, "anomaly_fanout": 0,
        "heartbeat_fanout": 0,
        "clients_connected": 0, "clients_total": 0,
    }
    p.captured = []     # type: ignore[attr-defined]

    async def _capture(payload):
        p.captured.append(payload)

    p._forward_to_app_rpc = _capture     # type: ignore[method-assign]
    return p


def _run(coro):
    return asyncio.new_event_loop().run_until_complete(coro)


# ─── handler return shapes ───
def test_formation_set_returns_ack_with_transition_id():
    p = _bare_proc()
    out = _run(p._h_formation_set({
        "type": "V_SHAPE", "d": 5.0, "theta_deg": 90.0}))
    assert out["ok"] is True
    assert isinstance(out["transition_id"], int)
    assert len(p.captured) == 1
    assert p.captured[0]["type"] == "formation_change"


def test_mission_start_requires_mission_id():
    p = _bare_proc()
    with pytest.raises(ValueError, match="mission_id"):
        _run(p._h_mission_start({}))
    assert p.captured == []


def test_mission_start_with_id_publishes_cmd():
    p = _bare_proc()
    out = _run(p._h_mission_start({
        "mission_id": "M-001",
        "waypoints": [(1.0, 2.0), (3.0, 4.0)],
    }))
    assert out == {"ok": True, "mission_handle": "M-001"}
    assert len(p.captured) == 1
    assert p.captured[0]["mission_id"] == "M-001"


def test_mission_abort_publishes_cmd():
    p = _bare_proc()
    out = _run(p._h_mission_abort({}))
    assert out == {"ok": True}
    assert len(p.captured) == 1
    assert p.captured[0]["type"] == "mission_abort"


def test_telemetry_subscribe_returns_default_rate():
    p = _bare_proc()
    out = _run(p._h_telemetry_subscribe({}))
    assert out["ok"] is True
    assert out["rate_hz"] == WsTelemetryProcess.TELEMETRY_HZ


def test_telemetry_subscribe_with_explicit_rate():
    p = _bare_proc()
    out = _run(p._h_telemetry_subscribe({"rate_hz": 10.0}))
    assert out["rate_hz"] == 10.0


def test_state_set_round_trips_phase():
    p = _bare_proc()
    out = _run(p._h_state_set({"phase": "READY"}))
    assert out == {"ok": True, "current_phase": "READY"}
    assert len(p.captured) == 1
    assert p.captured[0]["phase"] == "READY"


def test_leader_takeover_returns_election_started():
    p = _bare_proc()
    out = _run(p._h_leader_takeover({"new_leader_id": 7}))
    assert out["ok"] is True
    assert out["election_started"] is True


def test_map_upload_brief_returns_zero_kb_initially():
    p = _bare_proc()
    out = _run(p._h_map_upload_brief({
        "bbox": [37.49, 127.02, 37.50, 127.03],
        "mission_id": "M-001",
    }))
    assert out == {"ok": True, "downloaded_size_kb": 0}


def test_set_recording_returns_state():
    p = _bare_proc()
    out = _run(p._h_set_recording({"on": True}))
    assert out == {"ok": True, "recording": True}


# ─── dispatcher: parse errors / unknown methods ───
def test_dispatcher_parse_error_for_invalid_json():
    p = _bare_proc()
    captured: list[tuple] = []

    async def fake_send_error(rpc_id, code, message):
        captured.append((rpc_id, code, message))

    p._send_error = fake_send_error
    _run(p._handle_inbound("not-json"))
    assert captured and captured[0][1] == -32700


def test_dispatcher_unknown_method_returns_neg32601():
    p = _bare_proc()
    captured: list[tuple] = []

    async def fake_send_error(rpc_id, code, message):
        captured.append((rpc_id, code, message))

    p._send_error = fake_send_error
    _run(p._handle_inbound(
        '{"jsonrpc":"2.0","id":1,"method":"unknown.method"}'))
    assert captured and captured[0][1] == -32601


def test_dispatcher_handler_exception_returns_neg32000():
    p = _bare_proc()
    captured: list[tuple] = []

    async def fake_send_error(rpc_id, code, message):
        captured.append((rpc_id, code, message))

    p._send_error = fake_send_error
    _run(p._handle_inbound(
        '{"jsonrpc":"2.0","id":2,"method":"mission.start","params":{}}'))
    assert captured and captured[0][1] == -32000


def test_dispatcher_success_path_calls_send_result():
    p = _bare_proc()
    captured: list[tuple] = []

    async def fake_send_result(rpc_id, result):
        captured.append((rpc_id, result))

    p._send_result = fake_send_result
    _run(p._handle_inbound(
        '{"jsonrpc":"2.0","id":3,"method":"mission.abort","params":{}}'))
    assert captured and captured[0][1]["ok"] is True
