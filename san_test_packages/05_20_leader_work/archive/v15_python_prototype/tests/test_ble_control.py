"""
End-to-end tests for the TCP-based BLE GATT simulator.

We don't spawn the full BleControlProcess (mp.Process); instead we
exercise it as an in-process object with a real listening socket,
because all the protocol logic is in spawn-safe methods. A real client
TCP socket connects, sends JSON-line frames, and receives notifications.

The wire protocol must stay 1:1 with AIRYS BleSim — that's what makes
AIRYS-APP work against our robot without changes.
"""
from __future__ import annotations

import json
import multiprocessing as mp
import socket
import threading
import time
from unittest.mock import MagicMock

import pytest

from control.ble_control_process import (
    DEFAULT_BIND,
    BleControlProcess,
    PinAuthChallenge,
)
from control.state_machine import ErrorCode, Opcode, Phase
from core import make_topic_queues

# Pick a high port that's unlikely to clash; tests retry if it does
_TEST_PORT = 25555


def _make_proc(cfg_overrides=None) -> BleControlProcess:
    """Build a BleControlProcess instance with a real socket but no spawn."""
    cfg = MagicMock()
    overrides = cfg_overrides or {}
    overrides.setdefault(("ble", "bind_addr"), DEFAULT_BIND)
    overrides.setdefault(("ble", "tcp_port"), _TEST_PORT)
    overrides.setdefault(("system", "cpu_affinity"), None)

    def _get(*keys, default=None):
        return overrides.get(tuple(keys), default)
    cfg.get.side_effect = _get

    proc = BleControlProcess.__new__(BleControlProcess)
    # Set BaseProcess attrs we need
    proc.queues = make_topic_queues()
    proc.shutdown_event = mp.Event()
    proc.cfg = cfg
    proc.log = MagicMock()
    proc._listen_sock = None
    proc._client_sock = None
    proc._client_lock = None
    proc._client_addr = ("", 0)
    proc._last_seen_at = 0.0
    proc._stats = {"connects": 0, "disconnects": 0, "cmds": 0,
                    "settings": 0, "notifies_state": 0,
                    "notifies_creds": 0, "notifies_error": 0, "rejects": 0,
                    "pin_challenges": 0, "pin_success": 0, "pin_fail": 0}
    proc._pin_auth = PinAuthChallenge()
    # Stub spawn_thread + is_running
    proc._threads = []
    proc.spawn_thread = lambda target, name: threading.Thread(
        target=target, name=name, daemon=True).start() or None
    proc.is_running = lambda: not proc.shutdown_event.is_set()
    return proc


@pytest.fixture
def proc():
    global _TEST_PORT
    # Increment port between fixtures to avoid TIME_WAIT collisions
    _TEST_PORT += 1
    p = _make_proc()
    p.setup()
    time.sleep(0.1)        # let accept_loop start
    yield p
    p.shutdown_event.set()
    time.sleep(0.2)
    p.teardown()


def _connect_and_recv(port, timeout=2.0):
    """Open a client TCP socket and read the first newline-delimited JSON."""
    s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    s.settimeout(timeout)
    return s


def _send_line(s: socket.socket, obj: dict):
    s.sendall((json.dumps(obj) + "\n").encode("utf-8"))


def _recv_line(s: socket.socket, timeout=2.0) -> dict:
    s.settimeout(timeout)
    buf = b""
    while b"\n" not in buf:
        chunk = s.recv(256)
        if not chunk:
            raise EOFError("server closed before sending newline")
        buf += chunk
    line, _, _ = buf.partition(b"\n")
    return json.loads(line.decode())


# ════════════════════════════════════════════════════════════════
# Connection / disconnection
# ════════════════════════════════════════════════════════════════
def test_client_connects_and_receives_state_on_handshake(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    msg = _recv_line(s)
    assert msg["notify"] == "STATE"
    assert msg["val"] == int(Phase.BLE_CONN)
    s.close()
    time.sleep(0.05)
    assert proc._stats["connects"] >= 1


def test_second_client_replaces_first(proc):
    """BLE has one bonded peer — a second connect must close the first."""
    s1 = _connect_and_recv(_TEST_PORT)
    _send_line(s1, {"op": "connect"})
    _ = _recv_line(s1)         # consume the STATE notification

    s2 = _connect_and_recv(_TEST_PORT)
    _send_line(s2, {"op": "connect"})
    _ = _recv_line(s2)
    time.sleep(0.1)
    # First socket should have been closed by the server
    s1.settimeout(1.0)
    try:
        chunk = s1.recv(64)
        assert chunk == b""    # EOF
    except (ConnectionResetError, OSError):
        pass                   # also acceptable
    s2.close()


def test_disconnect_op_closes_client(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)
    _send_line(s, {"op": "disconnect"})
    time.sleep(0.1)
    assert proc._stats["disconnects"] >= 1
    s.close()


# ════════════════════════════════════════════════════════════════
# CMD writes (uplink to FSM via ble_command queue)
# ════════════════════════════════════════════════════════════════
def test_cmd_write_publishes_to_queue(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "write", "char": "CMD", "val": int(Opcode.WIFI_ON)})
    time.sleep(0.1)

    # Pull from the queue
    from core.ipc import consume
    msg = consume(proc.queues.ble_command, timeout=1.0)
    assert msg is not None
    assert msg["opcode"] == int(Opcode.WIFI_ON)
    assert msg["label"] == "WIFI_ON"
    s.close()


def test_invalid_cmd_value_rejected(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "write", "char": "CMD", "val": 999})  # > 0xFF
    _send_line(s, {"op": "write", "char": "CMD", "val": "abc"})
    time.sleep(0.1)
    assert proc._stats["rejects"] >= 2
    s.close()


def test_unknown_opcode_label_falls_back_to_hex(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "write", "char": "CMD", "val": 0xFF})  # not in Opcode
    time.sleep(0.1)
    from core.ipc import consume
    msg = consume(proc.queues.ble_command, timeout=1.0)
    assert msg is not None
    assert msg["opcode"] == 0xFF
    assert msg["label"] == "0xFF"
    s.close()


def test_settings_write_publishes_bytes(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    payload = bytes(range(16))
    _send_line(s, {"op": "write", "char": "SETTINGS", "hex": payload.hex()})
    time.sleep(0.1)
    from core.ipc import consume
    msg = consume(proc.queues.ble_settings, timeout=1.0)
    assert msg is not None
    assert msg["bytes"] == payload
    s.close()


def test_invalid_json_rejected_without_crash(proc):
    s = _connect_and_recv(_TEST_PORT)
    s.sendall(b"not valid json\n")
    s.sendall(b'{"op":"write"\n')           # truncated
    time.sleep(0.1)
    assert proc._stats["rejects"] >= 1
    # Server still accepting valid traffic
    _send_line(s, {"op": "connect"})
    msg = _recv_line(s)
    assert msg["notify"] == "STATE"
    s.close()


# ════════════════════════════════════════════════════════════════
# Notifications (downlink: phase / creds / errors)
# ════════════════════════════════════════════════════════════════
def test_phase_change_notifies_state(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)        # initial BLE_CONN STATE

    from core.ipc import publish
    publish(proc.queues.ble_phase, int(Phase.WIFI_BRINGUP))
    msg = _recv_line(s, timeout=2.0)
    assert msg["notify"] == "STATE"
    assert msg["val"] == int(Phase.WIFI_BRINGUP)
    s.close()


def test_wifi_creds_notification(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    from core.ipc import publish
    creds = {"ssid": "patrol-ABCD", "psk": "test1234",
              "ip": "192.168.42.1", "video_port": 5000,
              "ws_port": 5001, "transport": "srt_listener"}
    publish(proc.queues.ble_creds, creds)
    msg = _recv_line(s, timeout=2.0)
    assert msg["notify"] == "WIFI_CRED"
    assert msg["payload"]["ssid"] == "patrol-ABCD"
    assert msg["payload"]["video_port"] == 5000
    s.close()


def test_error_notification(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    from core.ipc import publish
    publish(proc.queues.ble_errors, int(ErrorCode.HOSTAPD_FAIL))
    msg = _recv_line(s, timeout=2.0)
    assert msg["notify"] == "ERROR"
    assert msg["code"] == int(ErrorCode.HOSTAPD_FAIL)
    s.close()


# ════════════════════════════════════════════════════════════════
# 0xFF05 PIN auth — wire-level integration with the BLE handler
# ════════════════════════════════════════════════════════════════
# These tests prove that PinAuthChallenge is reachable through the BLE
# control plane, that successful verification reaches the auth_state +
# audit buses, and that disconnect clears auth. Before this wiring, the
# PinAuthChallenge class was unit-tested in isolation but never called
# from the BLE process — see audit findings H1 / C1 for context.
import hashlib  # noqa: E402
import hmac  # noqa: E402


def _drain_audit(queues, timeout=0.5):
    """Collect every audit_event published within `timeout` seconds."""
    from core.ipc import consume
    out = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ev = consume(queues.audit_event, timeout=0.05)
        if ev is not None:
            out.append(ev)
    return out


def _hmac_response(challenge: bytes, pin: str = "1234") -> bytes:
    return hmac.new(pin.encode(), challenge, hashlib.sha256).digest()


def test_pin_auth_read_returns_32_byte_challenge(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "read", "char": "PIN_AUTH"})
    msg = _recv_line(s, timeout=2.0)
    assert msg["notify"] == "PIN_CHALLENGE"
    assert len(bytes.fromhex(msg["hex"])) == 32
    s.close()


def test_pin_auth_correct_response_emits_success_audit_and_auth_state(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "read", "char": "PIN_AUTH"})
    challenge_msg = _recv_line(s)
    challenge = bytes.fromhex(challenge_msg["hex"])

    response = _hmac_response(challenge)
    _send_line(s, {"op": "write", "char": "PIN_AUTH",
                   "hex": response.hex()})
    result = _recv_line(s, timeout=2.0)
    assert result["notify"] == "PIN_RESULT"
    assert result["ok"] is True

    # auth_state bus carries authenticated=True
    from core.ipc import consume
    state = consume(proc.queues.auth_state, timeout=1.0)
    assert state is not None
    assert state["authenticated"] is True
    assert state["reason"] == "pin_verified"

    # Audit bus has a permission/pin_auth_success entry
    events = _drain_audit(proc.queues)
    assert any(e["category"] == "permission"
               and e["event"] == "pin_auth_success"
               for e in events), events
    s.close()


def test_pin_auth_wrong_response_emits_fail_audit_only(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "read", "char": "PIN_AUTH"})
    _ = _recv_line(s)              # consume challenge

    _send_line(s, {"op": "write", "char": "PIN_AUTH",
                   "hex": ("00" * 32)})
    result = _recv_line(s, timeout=2.0)
    assert result["notify"] == "PIN_RESULT"
    assert result["ok"] is False
    assert result["locked_out"] is False

    # No auth_state should be published on failure — operator stays
    # unauthenticated by default.
    from core.ipc import consume
    assert consume(proc.queues.auth_state, timeout=0.3) is None

    events = _drain_audit(proc.queues)
    fails = [e for e in events
             if e["event"] == "pin_auth_fail"]
    assert len(fails) == 1
    s.close()


def test_pin_auth_lockout_emits_lockout_audit_on_third_failure(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    last_result: dict = {}
    for _ in range(3):
        _send_line(s, {"op": "read", "char": "PIN_AUTH"})
        _ = _recv_line(s)
        _send_line(s, {"op": "write", "char": "PIN_AUTH",
                       "hex": ("00" * 32)})
        last_result = _recv_line(s, timeout=2.0)

    assert last_result["ok"] is False
    assert last_result["locked_out"] is True

    events = _drain_audit(proc.queues)
    assert any(e["event"] == "pin_auth_lockout" for e in events), events

    # Subsequent read returns empty challenge (lockout window open)
    _send_line(s, {"op": "read", "char": "PIN_AUTH"})
    locked = _recv_line(s, timeout=2.0)
    assert locked["hex"] == ""
    s.close()


def test_disconnect_after_auth_resets_state_and_emits_audit(proc):
    s = _connect_and_recv(_TEST_PORT)
    _send_line(s, {"op": "connect"})
    _ = _recv_line(s)

    _send_line(s, {"op": "read", "char": "PIN_AUTH"})
    challenge = bytes.fromhex(_recv_line(s)["hex"])
    _send_line(s, {"op": "write", "char": "PIN_AUTH",
                   "hex": _hmac_response(challenge).hex()})
    _ = _recv_line(s)               # PIN_RESULT ok

    # Drain the "authenticated=True" so we can observe the reset cleanly.
    from core.ipc import consume
    _ = consume(proc.queues.auth_state, timeout=1.0)
    _ = _drain_audit(proc.queues)

    _send_line(s, {"op": "disconnect"})
    time.sleep(0.15)

    reset_state = consume(proc.queues.auth_state, timeout=1.0)
    assert reset_state is not None
    assert reset_state["authenticated"] is False

    events = _drain_audit(proc.queues)
    assert any(e["event"] == "pin_auth_reset_on_disconnect"
               for e in events), events
    s.close()
