"""
Tests for adapters/ntrip_process.py — NtripClientAdapter standalone
process per SDD §3.2.

Coverage:
  • setup() honours rtk.ntrip.enabled
  • RTCM frames received from NtripClient are forwarded to
    queues.rtcm_corrections
  • GGA published on queues.gga_latest is cached and exposed via the
    gga_source callable
  • Stale GGA (>30 s) is treated as None
  • RtkGnssAdapter writes RTCM frames pulled from the queue to its
    serial port — the queue is the only coupling between the two
    adapters now
"""
from __future__ import annotations

import threading
import time
from queue import Queue as ThreadQueue
from types import SimpleNamespace
from unittest.mock import MagicMock

from adapters.ntrip_process import NtripClientAdapter
from adapters.rtk_gnss import RtkGnssAdapter


def _make_ntrip(cfg_overrides: dict | None = None,
                gga_q: ThreadQueue | None = None,
                rtcm_q: ThreadQueue | None = None) -> NtripClientAdapter:
    """Construct without spawning. Skips BaseProcess machinery."""
    overrides = {("rtk", "ntrip", "enabled"): False}
    if cfg_overrides:
        overrides.update(cfg_overrides)
    cfg = MagicMock()
    cfg.get.side_effect = (
        lambda *k, default=None: overrides.get(tuple(k), default))

    proc = NtripClientAdapter.__new__(NtripClientAdapter)
    proc.cfg = cfg
    proc.queues = SimpleNamespace(
        gga_latest=gga_q or ThreadQueue(maxsize=4),
        rtcm_corrections=rtcm_q or ThreadQueue(maxsize=32),
    )
    proc._ntrip = None
    proc._stop_event = None
    proc._gga_lock = threading.Lock()
    proc._cached_gga = None
    proc._cached_gga_t = 0.0
    proc._enabled = False
    proc.log = MagicMock()
    return proc


def test_setup_idle_when_ntrip_disabled():
    proc = _make_ntrip()
    proc.setup()
    assert proc._enabled is False
    assert proc._ntrip is None


def test_setup_idle_when_host_or_mountpoint_missing():
    proc = _make_ntrip(cfg_overrides={
        ("rtk", "ntrip", "enabled"): True,
        ("rtk", "ntrip", "host"):    "",          # missing
        ("rtk", "ntrip", "mountpoint"): "",       # missing
    })
    # Won't actually start the NTRIP TCP thread because of the missing fields.
    # Bypass spawn_thread (BaseProcess mechanic) — replace with no-op.
    proc.spawn_thread = lambda *a, **kw: None
    proc.setup()
    assert proc._enabled is False
    assert proc._ntrip is None


def test_publish_rtcm_writes_to_queue():
    rtcm_q = ThreadQueue(maxsize=8)
    proc = _make_ntrip(rtcm_q=rtcm_q)
    proc._publish_rtcm(b"\xd3\x00\x10payload-bytes")
    assert not rtcm_q.empty()
    frame = rtcm_q.get_nowait()
    assert frame == b"\xd3\x00\x10payload-bytes"
    assert isinstance(frame, bytes)


def test_latest_gga_returns_none_before_any_received():
    proc = _make_ntrip()
    assert proc._latest_gga() is None


def test_latest_gga_caches_published_string():
    proc = _make_ntrip()
    proc._cached_gga = "$GPGGA,...*47\r\n"
    proc._cached_gga_t = time.monotonic()
    assert proc._latest_gga() == "$GPGGA,...*47\r\n"


def test_latest_gga_returns_none_when_stale():
    proc = _make_ntrip()
    proc._cached_gga = "$GPGGA,old\r\n"
    proc._cached_gga_t = time.monotonic() - 31.0      # >30 s old
    assert proc._latest_gga() is None


# ─── Contract test: RtkGnssAdapter consumes from rtcm_corrections ───
class _FakeSerial:
    def __init__(self):
        self.writes = []
    def write(self, b):
        self.writes.append(bytes(b))


def test_rtk_rtcm_consumer_writes_serial(monkeypatch):
    """The decoupling contract: NtripClientAdapter publishes a frame on
    queues.rtcm_corrections, and RtkGnssAdapter's consumer thread writes
    it to the GNSS serial port. We invoke the consumer directly with a
    short-lived loop so the test stays deterministic."""
    rtk = RtkGnssAdapter.__new__(RtkGnssAdapter)
    rtk.cfg = MagicMock()
    rtk._serial = _FakeSerial()
    rtk._serial_lock = threading.Lock()
    rtk.queues = SimpleNamespace(rtcm_corrections=ThreadQueue(maxsize=8))
    # Stub `is_running` so the consumer loop exits after one frame.
    n_iter = [0]
    def _running():
        n_iter[0] += 1
        return n_iter[0] <= 1     # True the first time, False after
    rtk.is_running = _running

    rtk.queues.rtcm_corrections.put(b"frame-1")
    rtk._rtcm_consumer()
    assert rtk._serial.writes == [b"frame-1"]
