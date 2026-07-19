"""
NTRIP client tests.

Two test layers:
  • RTCM3 framing — pure functions on bytes (CRC, frame extraction)
  • Integration   — spin up a fake NTRIP caster on localhost, verify
                    request format, GGA forwarding, RTCM delivery
"""
from __future__ import annotations

import socket
import threading
import time

import pytest

from adapters.ntrip_client import (
    RTCM3_PREAMBLE,
    NtripClient,
    NtripConfig,
    crc24q,
    parse_rtcm3_frame,
)


# ────────────── RTCM3 framing ──────────────
def _make_frame(payload: bytes) -> bytes:
    """Build a valid RTCM3 frame around `payload`."""
    n = len(payload)
    assert n < 1024
    header = bytes([RTCM3_PREAMBLE, (n >> 8) & 0x03, n & 0xFF])
    body = header + payload
    c = crc24q(body)
    return body + bytes([(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF])


def test_crc24q_known_vector():
    """Spot check: CRC-24Q of empty input is 0."""
    assert crc24q(b"") == 0


def test_parse_extracts_clean_frame():
    payload = b"\x12\x34" + b"\xAB" * 8
    frame = _make_frame(payload)
    res = parse_rtcm3_frame(frame)
    assert res is not None
    consumed, got, leftover = res
    assert consumed == len(frame)
    assert got == frame
    assert leftover == b""


def test_parse_returns_none_when_incomplete():
    payload = b"\x12" * 100
    frame = _make_frame(payload)
    # Truncate mid-payload
    res = parse_rtcm3_frame(frame[:50])
    assert res is None


def test_parse_skips_garbage_before_preamble():
    payload = b"\x00\x01\x02"
    frame = _make_frame(payload)
    buf = b"\xAA\xBB\xCC" + frame
    res = parse_rtcm3_frame(buf)
    assert res is not None
    consumed, got, leftover = res
    # Garbage skipped: returns (0, b"", buf_starting_at_preamble)
    assert consumed == 0
    assert got == b""
    assert leftover == frame


def test_parse_drops_corrupted_frame_and_resyncs():
    """Frame with bad CRC must be discarded; parser resyncs at next preamble."""
    payload = b"\x42" * 5
    frame = bytearray(_make_frame(payload))
    frame[-1] ^= 0xFF                                # corrupt CRC
    next_payload = b"\x99" * 3
    next_frame = _make_frame(next_payload)
    buf = bytes(frame) + next_frame
    res = parse_rtcm3_frame(buf)
    assert res is not None
    consumed, got, leftover = res
    # Bad frame yields (0, b"", buf_minus_one_byte_to_resync)
    assert consumed == 0
    assert got == b""
    # Eventually we should reach the good frame
    res2 = parse_rtcm3_frame(leftover)
    while res2 is not None and res2[1] == b"":
        if not res2[2] and res2[0] == 0:
            pytest.fail("resync lost data without finding next frame")
        res2 = parse_rtcm3_frame(res2[2])
        if res2 is None:
            pytest.fail("ran out of buffer before finding good frame")
    assert res2[1] == next_frame


def test_parse_handles_two_back_to_back_frames():
    f1 = _make_frame(b"\x01\x02")
    f2 = _make_frame(b"\x03\x04\x05")
    buf = f1 + f2
    res1 = parse_rtcm3_frame(buf)
    assert res1[1] == f1
    res2 = parse_rtcm3_frame(res1[2])
    assert res2[1] == f2


# ────────────── Integration: fake NTRIP caster ──────────────
class _FakeCaster:
    """Minimal caster: accepts a connection, records the request, sends 200 OK
    + (optionally) some RTCM frames.

    Run in a background thread; pass the assigned port to the client.
    """
    def __init__(self, frames: list[bytes],
                 require_auth: str | None = None,
                 expect_gga: bool = False):
        self.frames = frames
        self.require_auth = require_auth
        self.expect_gga = expect_gga

        self.received_request_lines: list[str] = []
        self.received_gga: list[str] = []
        self.serve_done = threading.Event()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self.port = self._sock.getsockname()[1]
        self._sock.listen(1)
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def start(self):
        self._thread.start()

    def stop(self):
        try:
            self._sock.close()
        except Exception:
            pass

    def _serve(self):
        try:
            self._sock.settimeout(5.0)
            conn, _ = self._sock.accept()
        except Exception:
            return
        try:
            conn.settimeout(2.0)
            # Read request header until \r\n\r\n
            buf = bytearray()
            while b"\r\n\r\n" not in buf:
                chunk = conn.recv(1024)
                if not chunk:
                    return
                buf += chunk
            self.received_request_lines = bytes(buf).decode().split("\r\n")
            if self.require_auth and self.require_auth not in bytes(buf).decode():
                conn.sendall(b"HTTP/1.1 401 Unauthorized\r\n\r\n")
                return
            conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: gnss/data\r\n\r\n")
            # Send frames
            for f in self.frames:
                conn.sendall(f)
                time.sleep(0.05)
            # Optionally read GGAs from client
            if self.expect_gga:
                conn.settimeout(1.5)
                try:
                    extra = conn.recv(4096).decode("ascii", "ignore")
                    for line in extra.split("\r\n"):
                        if line.startswith("$GPGGA") or line.startswith("$GNGGA"):
                            self.received_gga.append(line)
                except socket.timeout:
                    pass
            time.sleep(0.2)
        finally:
            self.serve_done.set()
            try:
                conn.close()
            except Exception:
                pass


@pytest.fixture
def fake_caster_factory():
    """Yields a builder; auto-stops all casters at teardown."""
    casters: list[_FakeCaster] = []
    def make(**kwargs):
        c = _FakeCaster(**kwargs)
        c.start()
        casters.append(c)
        return c
    yield make
    for c in casters:
        c.stop()


def test_client_sends_correct_get_request(fake_caster_factory):
    caster = fake_caster_factory(frames=[])
    received: list[bytes] = []
    stop = threading.Event()
    cfg = NtripConfig(host="127.0.0.1", port=caster.port,
                      mountpoint="TESTMP", username="alice", password="pw",
                      socket_timeout_s=2.0)
    client = NtripClient(cfg, rtcm_sink=received.append, stop_event=stop)
    client.start()
    caster.serve_done.wait(timeout=5.0)
    stop.set()
    client.join(timeout=2.0)

    lines = caster.received_request_lines
    assert any(line.startswith("GET /TESTMP HTTP/1.1") for line in lines)
    # Authorization: Basic <base64(alice:pw)>
    auth_line = next(
        (line for line in lines if line.startswith("Authorization:")), "")
    assert "Basic " in auth_line
    import base64
    expected = base64.b64encode(b"alice:pw").decode()
    assert expected in auth_line
    # NTRIP version header is mandatory for 2.0
    assert any("Ntrip-Version" in line for line in lines)


def test_client_delivers_rtcm_frames_to_sink(fake_caster_factory):
    f1 = _make_frame(b"\xAA\xBB")
    f2 = _make_frame(b"\xCC\xDD\xEE")
    caster = fake_caster_factory(frames=[f1, f2])
    received: list[bytes] = []
    stop = threading.Event()
    cfg = NtripConfig(host="127.0.0.1", port=caster.port,
                      mountpoint="X", socket_timeout_s=2.0)
    client = NtripClient(cfg, rtcm_sink=received.append, stop_event=stop)
    client.start()
    # Wait for both frames or 2s
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline and len(received) < 2:
        time.sleep(0.05)
    stop.set()
    client.join(timeout=2.0)
    assert len(received) == 2
    assert received[0] == f1
    assert received[1] == f2


def test_client_forwards_gga_to_caster(fake_caster_factory):
    """VRS scenario: client must forward our GGA so caster can compute corrections."""
    caster = fake_caster_factory(frames=[], expect_gga=True)
    sample_gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
    stop = threading.Event()
    cfg = NtripConfig(host="127.0.0.1", port=caster.port,
                      mountpoint="VRS", socket_timeout_s=3.0,
                      gga_period_s=0.1)               # send fast for test
    client = NtripClient(cfg,
                         rtcm_sink=lambda b: None,
                         gga_source=lambda: sample_gga,
                         stop_event=stop)
    client.start()
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline and not caster.received_gga:
        time.sleep(0.05)
    stop.set()
    client.join(timeout=2.0)
    assert caster.received_gga, "caster never saw a GGA forward"
    assert "$GPGGA" in caster.received_gga[0]


def test_client_reconnects_after_caster_dies():
    """Client must retry after connection loss (basic resilience).

    We only verify the reconnect counter increments — frame delivery in
    a brief connection is racy on slow CI, and is covered separately
    by test_client_delivers_rtcm_frames_to_sink.
    """
    f = _make_frame(b"\x01")
    c1 = _FakeCaster(frames=[f])
    c1.start()
    received: list[bytes] = []
    stop = threading.Event()
    cfg = NtripConfig(host="127.0.0.1", port=c1.port, mountpoint="X",
                      initial_backoff_s=0.05, max_backoff_s=0.1,
                      socket_timeout_s=1.0)
    client = NtripClient(cfg, rtcm_sink=received.append, stop_event=stop)
    client.start()
    # Wait for first session to complete
    c1.serve_done.wait(timeout=2.0)
    c1.stop()
    # Wait for reconnect attempt
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline and client.stats["reconnects"] == 0:
        time.sleep(0.05)
    stop.set()
    client.join(timeout=2.0)
    assert client.stats["reconnects"] >= 1, \
        "client should have attempted at least one reconnect"
