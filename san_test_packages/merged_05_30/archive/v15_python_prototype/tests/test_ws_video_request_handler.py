"""Tests for the video.request JSON-RPC handler wiring.

Companion to tests/test_ws_jsonrpc.py — covers the new
``video.request`` method that bridges Android-app JSON-RPC frames to
the typed `VideoRequest` queue OrchestratorProcess already consumes.
"""
from __future__ import annotations

import asyncio
import json
import multiprocessing as mp
import threading
from unittest.mock import MagicMock

import pytest

from control.ws_telemetry_process import WsTelemetryProcess
from core.messages import (
    VIDEO_ACTION_CHANGE_QUALITY,
    VIDEO_ACTION_START,
    VIDEO_ACTION_STOP,
    VideoRequest,
)


def _bare_proc() -> WsTelemetryProcess:
    """Synchronous WsTelemetryProcess for handler-only assertions."""
    p = WsTelemetryProcess.__new__(WsTelemetryProcess)
    p.queues = MagicMock()
    p.queues.tablet_video_request = mp.Queue(maxsize=8)
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
        "broadcasts": 0, "anomaly_fanout": 0, "heartbeat_fanout": 0,
        "clients_connected": 0, "clients_total": 0,
    }
    return p


def _run(coro):
    return asyncio.new_event_loop().run_until_complete(coro)


# ─── handler behaviour ─────────────────────────────────────────────────


def test_video_request_start_publishes_typed_message():
    p = _bare_proc()
    out = _run(p._h_video_request({
        "sequence": 7,
        "target_robot_id": 2,
        "protocol": "srt",
        "desired_port": 5800,
        "codec": "h265",
        "quality": "fhd",
        "action": VIDEO_ACTION_START,
    }))
    assert out == {
        "ok": True, "sequence": 7,
        "target_robot_id": 2, "action": VIDEO_ACTION_START,
    }
    msg = p.queues.tablet_video_request.get(timeout=0.5)
    assert isinstance(msg, VideoRequest)
    assert msg.sequence == 7
    assert msg.target_robot_id == 2
    assert msg.action == VIDEO_ACTION_START
    assert msg.quality == "fhd"


def test_video_request_stop_propagates():
    p = _bare_proc()
    _run(p._h_video_request({
        "sequence": 11, "target_robot_id": 1,
        "action": VIDEO_ACTION_STOP,
    }))
    msg = p.queues.tablet_video_request.get(timeout=0.5)
    assert msg.action == VIDEO_ACTION_STOP


def test_video_request_change_quality_keeps_sequence_in_ack():
    p = _bare_proc()
    out = _run(p._h_video_request({
        "sequence": 99, "target_robot_id": 3,
        "action": VIDEO_ACTION_CHANGE_QUALITY,
        "quality": "thumbnail",
    }))
    assert out["sequence"] == 99
    assert out["action"] == VIDEO_ACTION_CHANGE_QUALITY


def test_video_request_invalid_action_surfaces_validation_error():
    """parse_video_request calls validate(); an unknown action should
    bubble up as a ValueError that the dispatcher converts to a
    -32000 JSON-RPC error frame."""
    p = _bare_proc()
    with pytest.raises(Exception) as exc_info:
        _run(p._h_video_request({
            "sequence": 1, "target_robot_id": 1,
            "action": "not_a_real_action",
        }))
    # The exact exception type is set by VideoRequest.validate() —
    # ValueError or AssertionError, depending on the dataclass — but
    # the error message should reference the bad action.
    assert "action" in str(exc_info.value).lower()
    # Nothing was published.
    assert p.queues.tablet_video_request.empty()


# ─── dispatcher integration ────────────────────────────────────────────


def test_dispatcher_routes_video_request_to_handler():
    """Drive the JSON-RPC dispatcher directly to confirm
    ``video.request`` is registered (not unknown-method -32601)."""
    p = _bare_proc()
    captured = []

    async def _capture_result(rpc_id, result):
        captured.append(("result", rpc_id, result))

    async def _capture_error(rpc_id, code, message):
        captured.append(("error", rpc_id, code, message))

    p._send_result = _capture_result  # type: ignore[method-assign]
    p._send_error  = _capture_error   # type: ignore[method-assign]

    _run(p._handle_inbound(json.dumps({
        "jsonrpc": "2.0", "id": 42, "method": "video.request",
        "params": {"sequence": 5, "target_robot_id": 1,
                   "action": VIDEO_ACTION_START},
    })))

    assert len(captured) == 1
    kind, rpc_id, result = captured[0]
    assert kind == "result"
    assert rpc_id == 42
    assert result["ok"] is True
    assert result["sequence"] == 5
