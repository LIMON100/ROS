"""
NtripClientAdapter — SDD §3.2 standalone NTRIP client process.

Previously the NtripClient was a thread spawned inside RtkGnssAdapter,
which coupled the two: any failure in NTRIP could affect RTK serial,
and the adapter list in §3.2 (17 processes) was off by one.

Architecture after the split:

    NtripClientAdapter (this file)            RtkGnssAdapter
    ──────────────────────────────            ──────────────
    NtripClient thread (TCP→caster)           Owns serial /dev/ttyACM0
        rtcm_sink   ────────────►  rtcm_corrections (mp.Queue)  ──►  serial.write
        gga_source  ◄──────────────  gga_latest      (mp.Queue)  ◄──  $GxGGA capture

The NtripClient class itself is unchanged — we just relocate its
hosting from a worker thread of RtkGnssAdapter to its own BaseProcess.

Falls back to a no-op when ntrip.enabled is false in config.
"""
from __future__ import annotations

import logging
import threading
import time
from typing import Optional

from core.base_process import BaseProcess
from core.ipc import consume, publish

from .ntrip_client import NtripClient, NtripConfig

log = logging.getLogger(__name__)


class NtripClientAdapter(BaseProcess):
    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="NtripClientAdapter",
            shutdown_event=shutdown_event,
            rate_hz=0.2,                  # step() prints periodic stats only
            cpu_affinity=config.get("system", "cpu_affinity", "ntrip") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._ntrip: Optional[NtripClient] = None
        self._stop_event: Optional[threading.Event] = None
        self._gga_lock: Optional[threading.Lock] = None
        self._cached_gga: Optional[str] = None
        self._cached_gga_t: float = 0.0
        self._enabled: bool = False

    # ── Lifecycle ─────────────────────────────────────────────────
    def setup(self) -> None:
        self._gga_lock = threading.Lock()
        self._enabled = bool(self.cfg.get("rtk", "ntrip", "enabled", default=False))
        if not self._enabled:
            self.log.info("NTRIP disabled in config — adapter idle")
            return

        nt_cfg = NtripConfig(
            host=self.cfg.get("rtk", "ntrip", "host", default=""),
            port=int(self.cfg.get("rtk", "ntrip", "port", default=2101)),
            mountpoint=self.cfg.get("rtk", "ntrip", "mountpoint", default=""),
            username=self.cfg.get("rtk", "ntrip", "username", default=""),
            password=self.cfg.get("rtk", "ntrip", "password", default=""),
        )
        if not nt_cfg.host or not nt_cfg.mountpoint:
            self.log.warning("NTRIP enabled but host/mountpoint missing — staying idle")
            self._enabled = False
            return

        # Cache the latest GGA so NtripClient's gga_source callable doesn't
        # block on consume() inside its own loop.
        self.spawn_thread(self._gga_consumer, name="GgaCnsm")

        self._stop_event = threading.Event()
        self._ntrip = NtripClient(
            nt_cfg,
            rtcm_sink=self._publish_rtcm,
            gga_source=self._latest_gga,
            stop_event=self._stop_event,
        )
        self._ntrip.start()
        self.log.info(
            f"NtripClient started — {nt_cfg.host}:{nt_cfg.port}/{nt_cfg.mountpoint}")

    def step(self) -> None:
        # Periodic stats (~every 5 s at rate_hz=0.2)
        if self._ntrip is not None and self._enabled:
            stats = self._ntrip.stats
            self.log.debug(
                f"ntrip frames={stats['frames']} bytes={stats['bytes']} "
                f"reconnects={stats['reconnects']} bad_crc={stats['bad_crc']}")

    def teardown(self) -> None:
        if self._stop_event is not None:
            self._stop_event.set()
        if self._ntrip is not None:
            self._ntrip.join(timeout=3.0)

    # ── Wires into the IPC queues ────────────────────────────────
    def _publish_rtcm(self, frame: bytes) -> None:
        """rtcm_sink for NtripClient — RTCM3 frame → RtkGnssAdapter serial."""
        publish(self.queues.rtcm_corrections, bytes(frame))

    def _gga_consumer(self) -> None:
        """RtkGnssAdapter publishes the latest $GxGGA every cycle; we
        cache the freshest one. NtripClient pulls via _latest_gga()."""
        while self.is_running():
            g = consume(self.queues.gga_latest, timeout=0.5)
            if g is None:
                continue
            with self._gga_lock:
                self._cached_gga = g
                self._cached_gga_t = time.monotonic()

    def _latest_gga(self) -> Optional[str]:
        """gga_source for NtripClient — returns None if too stale."""
        with self._gga_lock:
            if self._cached_gga is None:
                return None
            if (time.monotonic() - self._cached_gga_t) > 30.0:
                return None
            return self._cached_gga
