"""Tests for BLE GATT API (P1-9, SDD Rev.A.6 §15.2).

Pairs the BleSim TCP simulator (tests/blesim_tcp.py) with the
PinAuthChallenge logic in control/ble_control_process.py.
"""
from __future__ import annotations

import hashlib
import hmac
import json
import socket
import time

import pytest

from control.ble_control_process import PinAuthChallenge
from tests.blesim_tcp import BleSimServer


@pytest.fixture
def ble_sim():
    """Spin up BleSim on a free port; yield the running server."""
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    free_port = sock.getsockname()[1]
    sock.close()

    s = BleSimServer(host="127.0.0.1", port=free_port)
    s.start()
    yield s
    s.stop()


def _send_recv(host: str, port: int, msg: dict,
               timeout: float = 1.0) -> dict:
    """One-shot synchronous request/response over the BleSim TCP link."""
    sock = socket.socket()
    sock.settimeout(timeout)
    sock.connect((host, port))
    sock.sendall(json.dumps(msg).encode() + b"\n")
    chunks: list[bytes] = []
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        chunks.append(chunk)
        if b"\n" in chunk:
            break
    sock.close()
    line = b"".join(chunks).split(b"\n", 1)[0]
    return json.loads(line.decode())


# ─── BleSim transport tests ───
def test_blesim_starts_and_accepts_connection(ble_sim):
    """Smoke: a TCP client can connect without error."""
    sock = socket.socket()
    sock.settimeout(1.0)
    sock.connect((ble_sim.host, ble_sim.port))
    sock.close()


def test_0xff00_device_info_read_returns_32_bytes(ble_sim):
    resp = _send_recv(ble_sim.host, ble_sim.port,
                      {"op": "read", "uuid": 0xFF00})
    assert "data" in resp
    assert len(bytes.fromhex(resp["data"])) == 32


def test_0xff01_wifi_provisioning_write_invokes_handler(ble_sim):
    received: dict = {}
    ble_sim.on_write(0xFF01,
                     lambda data: received.setdefault("data", data))
    payload = json.dumps({
        "wifi_ssid": "SAN-MESH",
        "wifi_psk": "test123",
    }).encode()
    resp = _send_recv(ble_sim.host, ble_sim.port,
                      {"op": "write", "uuid": 0xFF01,
                       "data": payload.hex()})
    assert resp.get("ack") is True
    # Handler is dispatched synchronously inside the request, so the
    # payload is observable as soon as the ACK arrives.
    assert b"SAN-MESH" in received.get("data", b"")


# ─── PinAuthChallenge logic ───
def test_pin_auth_correct_response_succeeds():
    pa = PinAuthChallenge()
    pa.DEV_DEFAULT_PIN = "1234"
    challenge = pa.generate_challenge()
    assert len(challenge) == 32
    correct = hmac.new(b"1234", challenge, hashlib.sha256).digest()
    assert pa.verify_response(correct) is True
    assert pa.is_authenticated is True


def test_pin_auth_wrong_response_fails():
    pa = PinAuthChallenge()
    pa.DEV_DEFAULT_PIN = "1234"
    pa.generate_challenge()
    assert pa.verify_response(b"\x00" * 32) is False
    assert pa.is_authenticated is False


def test_pin_auth_lockout_after_3_failures():
    pa = PinAuthChallenge()
    pa.DEV_DEFAULT_PIN = "1234"
    for _ in range(3):
        pa.generate_challenge()
        pa.verify_response(b"\x00" * 32)
    # 4th attempt should be locked out — challenge returns b""
    assert pa.generate_challenge() == b""


def test_pin_auth_reset_on_disconnect():
    pa = PinAuthChallenge()
    pa.DEV_DEFAULT_PIN = "1234"
    challenge = pa.generate_challenge()
    correct = hmac.new(b"1234", challenge, hashlib.sha256).digest()
    pa.verify_response(correct)
    assert pa.is_authenticated is True
    pa.reset()
    assert pa.is_authenticated is False
    # Challenge buffer cleared too — re-issue is a fresh nonce.
    assert pa._challenge == b""


def test_pin_auth_lockout_clears_after_window():
    """Lockout window expires; challenge generation resumes."""
    pa = PinAuthChallenge()
    pa.DEV_DEFAULT_PIN = "1234"
    pa.LOCKOUT_S = 0.05            # squeeze the window for the test
    for _ in range(3):
        pa.generate_challenge()
        pa.verify_response(b"\x00" * 32)
    assert pa.generate_challenge() == b""
    time.sleep(0.06)
    fresh = pa.generate_challenge()
    assert len(fresh) == 32
