"""
End-to-end tests for WsTelemetryProcess.

We don't spawn the full BaseProcess — we instantiate via __new__ and
call setup() in-thread. Then a real websockets client connects and we
verify telemetry broadcast + JSON-RPC inbound forwarding.
"""
from __future__ import annotations

import asyncio
import json
import multiprocessing as mp
import threading
import time
from unittest.mock import MagicMock

import numpy as np
import pytest
import websockets

from control.ws_telemetry_process import WsTelemetryProcess
from core import make_topic_queues
from core.ipc import consume, publish
from core.messages import Header, Pose6D, RobotStatus

_TEST_PORT = 25600


def _make_proc(port: int):
    queues = make_topic_queues()
    cfg = MagicMock()
    cfg.get.side_effect = lambda *k, default=None: {
        ("ws", "port"): port,
        ("ws", "bind"): "127.0.0.1",
        ("system", "cpu_affinity", "comm"): None,
    }.get(tuple(k), default)

    p = WsTelemetryProcess.__new__(WsTelemetryProcess)
    p.queues = queues
    p.shutdown_event = mp.Event()
    p.cfg = cfg
    p.log = MagicMock()
    p._port = port
    p._loop = None
    p._loop_thread = None
    p._server_task = None
    p._clients = set()
    p._clients_lock = None
    p._snapshot_lock = None
    p._snapshot = {
        "phase": 0, "pose": None, "battery_soc": 1.0,
        "locomotion_mode": "stand", "mission": None, "ts_mono": 0.0,
    }
    p._stats = {"clients_connected": 0, "clients_total": 0,
                "broadcasts": 0, "rpc_received": 0, "rpc_errors": 0}
    p.spawn_thread = lambda target, name: threading.Thread(
        target=target, name=name, daemon=True).start()
    p.is_running = lambda: not p.shutdown_event.is_set()
    p.setup()
    time.sleep(0.6)        # asyncio loop + bind
    return p, queues


@pytest.fixture
def proc():
    global _TEST_PORT
    _TEST_PORT += 1
    p, queues = _make_proc(_TEST_PORT)
    yield p, queues, _TEST_PORT
    p.shutdown_event.set()
    p.teardown()
    time.sleep(0.2)


# ════════════════════════════════════════════════════════════════
# Connection
# ════════════════════════════════════════════════════════════════
def test_client_can_connect(proc):
    _, _, port = proc

    async def _connect():
        async with websockets.connect(f"ws://127.0.0.1:{port}"):
            return True

    ok = asyncio.run(_connect())
    assert ok is True


# ════════════════════════════════════════════════════════════════
# Telemetry broadcast
# ════════════════════════════════════════════════════════════════
def test_broadcast_includes_battery_after_status_published(proc):
    p, queues, port = proc
    publish(queues.robot_status, RobotStatus(
        header=Header.now(), battery_soc=0.42,
        motor_temp_max=30.0, locomotion_mode="walk", fault_codes=()))
    time.sleep(0.3)        # let _status_consumer pick it up

    async def _read_one():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            # First message arrives within ~33ms (30 Hz pump)
            raw = await asyncio.wait_for(ws.recv(), timeout=2.0)
            return json.loads(raw)

    msg = asyncio.run(_read_one())
    assert msg["type"] == "telemetry"
    assert msg["data"]["battery_soc"] == pytest.approx(0.42)
    assert msg["data"]["locomotion_mode"] == "walk"


def test_broadcast_includes_phase_from_ws_phase_queue(proc):
    p, queues, port = proc
    from control.state_machine import Phase
    publish(queues.ws_phase, int(Phase.WIFI_BRINGUP))
    time.sleep(0.3)

    async def _read_one():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            raw = await asyncio.wait_for(ws.recv(), timeout=2.0)
            return json.loads(raw)

    msg = asyncio.run(_read_one())
    assert msg["data"]["phase"] == int(Phase.WIFI_BRINGUP)


def test_broadcast_includes_pose(proc):
    p, queues, port = proc
    pose = Pose6D(
        header=Header.now(frame_id="map"),
        position=np.array([5.0, 10.0, 0.0], dtype=np.float32),
        orientation=np.array([0, 0, 0, 1], dtype=np.float32),
    )
    publish(queues.pose, pose)
    time.sleep(0.3)

    async def _read_one():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            raw = await asyncio.wait_for(ws.recv(), timeout=2.0)
            return json.loads(raw)

    msg = asyncio.run(_read_one())
    pose_field = msg["data"]["pose"]
    assert pose_field is not None
    assert pose_field[0] == pytest.approx(5.0)
    assert pose_field[1] == pytest.approx(10.0)


# ════════════════════════════════════════════════════════════════
# JSON-RPC inbound
# ════════════════════════════════════════════════════════════════
def test_inbound_rpc_forwarded_to_app_rpc_queue(proc):
    """P1-10 dispatcher: set_recording handler emits a typed cmd dict
    (not the legacy {"method": ..., "params": ...} envelope)."""
    p, queues, port = proc
    rpc_msg = {"jsonrpc": "2.0", "id": 1,
                "method": "set_recording", "params": {"on": True}}

    async def _send():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            await ws.send(json.dumps(rpc_msg))
            await asyncio.sleep(0.2)

    asyncio.run(_send())

    forwarded = consume(queues.app_rpc, timeout=1.0)
    assert forwarded is not None
    assert forwarded["type"] == "set_recording"
    assert forwarded["on"] is True


def test_invalid_json_rejected_without_disconnect(proc):
    """P1-10 dispatcher: invalid JSON returns -32700 but the connection
    stays up. The follow-up valid call (a real method) reaches app_rpc."""
    p, queues, port = proc

    async def _send():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            await ws.send("not valid json")
            await asyncio.sleep(0.1)
            await ws.send(json.dumps({
                "jsonrpc": "2.0", "id": 1,
                "method": "set_recording", "params": {"on": False}}))
            await asyncio.sleep(0.2)

    asyncio.run(_send())
    assert p._stats["rpc_errors"] >= 1
    forwarded = consume(queues.app_rpc, timeout=1.0)
    assert forwarded is not None
    assert forwarded["type"] == "set_recording"
    assert forwarded["on"] is False


def test_method_missing_rpc_silently_dropped(proc):
    p, queues, port = proc

    async def _send():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws:
            await ws.send(json.dumps({"jsonrpc": "2.0"}))   # no "method"
            await asyncio.sleep(0.1)

    asyncio.run(_send())
    # Counter increments but nothing forwarded
    assert consume(queues.app_rpc, timeout=0.2) is None


# ════════════════════════════════════════════════════════════════
# Multi-client fan-out
# ════════════════════════════════════════════════════════════════
def test_two_clients_both_receive_broadcast(proc):
    p, queues, port = proc
    publish(queues.robot_status, RobotStatus(
        header=Header.now(), battery_soc=0.7,
        motor_temp_max=30.0, locomotion_mode="walk", fault_codes=()))
    time.sleep(0.3)

    async def _read_two():
        async with websockets.connect(f"ws://127.0.0.1:{port}") as ws1, \
                    websockets.connect(f"ws://127.0.0.1:{port}") as ws2:
            await asyncio.sleep(0.2)
            r1 = await asyncio.wait_for(ws1.recv(), timeout=2.0)
            r2 = await asyncio.wait_for(ws2.recv(), timeout=2.0)
            return json.loads(r1), json.loads(r2)

    m1, m2 = asyncio.run(_read_two())
    assert m1["data"]["battery_soc"] == pytest.approx(0.7)
    assert m2["data"]["battery_soc"] == pytest.approx(0.7)
