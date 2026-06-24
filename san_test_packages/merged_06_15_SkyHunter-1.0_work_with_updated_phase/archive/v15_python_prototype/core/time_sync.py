"""Time synchronization — PTP IEEE-1588 + GNSS PPS (SDD Rev.A.6 §5.8).

Hierarchy of time sources, best to worst:
  1. GNSS PPS 1pps  — ±100 ns absolute, kernel timekeeper
  2. PTP master sync — ±1 ms relative, mesh internal
  3. NTP            — ±10 ms, development fallback
  4. RTC drift      — ~1 ms/min, last resort after master loss

Why ⭐ critical: without ms-precision cross-robot time alignment, P1-3
1-second predictive broadcast can't keep KPP §2.1.1 「control latency
≤ 150 ms」. Mission start blocks if PTP offset > 5 ms (SDD §5.8.2).
"""
from __future__ import annotations

import subprocess
import threading
from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, Optional, Tuple


class TimeSyncSource(IntEnum):
    NONE = 0
    RTC_DRIFT = 1     # last sync moment, drifting
    NTP = 2
    PTP_SLAVE = 3
    PTP_MASTER = 4
    GNSS_PPS = 5      # best


@dataclass
class TimeSyncStatus:
    source: TimeSyncSource
    offset_ms: float
    last_sync_age_s: float
    is_master: bool
    peers_synced: int
    error_message: str = ""


# Runner type: (argv: list[str], timeout: float) -> (rc, stdout, stderr)
RunnerFn = Callable[[list, float], Tuple[int, str, str]]


class PtpController:
    """Wraps linuxptp daemons (ptp4l, phc2sys) on the local node.

    The `runner` injection lets tests stub subprocess calls without
    touching the real daemon — production uses the bundled
    `_real_run` which shells out via subprocess.run.
    """

    PTP_MAX_OFFSET_MS = 5.0       # mission abort threshold
    PTP_WARN_OFFSET_MS = 1.0      # operator warn threshold
    MONITOR_PERIOD_S = 2.0
    STALE_THRESHOLD_S = 30.0      # demote to RTC_DRIFT after this

    def __init__(self,
                 interface: str = "wlan0",
                 is_master: bool = False,
                 runner: Optional[RunnerFn] = None):
        self.interface = interface
        self.is_master = is_master
        self._runner: RunnerFn = runner or self._real_run
        self._status = TimeSyncStatus(
            source=TimeSyncSource.NONE,
            offset_ms=999.0,
            last_sync_age_s=999.0,
            is_master=is_master,
            peers_synced=0,
        )
        self._lock = threading.Lock()
        self._monitor_thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    # ─── Subprocess plumbing ───
    @staticmethod
    def _real_run(cmd: list,
                  timeout: float = 5.0) -> Tuple[int, str, str]:
        try:
            r = subprocess.run(cmd, capture_output=True,
                               timeout=timeout, text=True)
            return r.returncode, r.stdout, r.stderr
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            return -1, "", str(e)

    # ─── Lifecycle ───
    def start(self) -> None:
        """Launch ptp4l and start the monitor thread.

        Master mode omits `-s`; slave mode adds it. The runner result is
        intentionally ignored for `start()` itself — on a dev box ptp4l
        will fail, but we still want the monitor thread alive so tests
        and the mission PreCheck see a consistent status object.
        """
        cmd = ["ptp4l", "-i", self.interface, "-m", "-2", "-H"]
        if not self.is_master:
            cmd.append("-s")
        self._runner(cmd, 0.5)

        self._stop.clear()
        self._monitor_thread = threading.Thread(
            target=self._monitor_loop, daemon=True, name="PtpMonitor")
        self._monitor_thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._monitor_thread is not None:
            self._monitor_thread.join(timeout=2.0)

    # ─── Monitor loop ───
    def _monitor_loop(self) -> None:
        while not self._stop.is_set():
            try:
                self._update_status()
            except Exception:        # noqa: BLE001 — never crash the thread
                pass
            self._stop.wait(self.MONITOR_PERIOD_S)

    def _update_status(self) -> None:
        rc, out, _ = self._runner(
            ["pmc", "-u", "GET TIME_STATUS_NP"], 1.0)
        if rc == 0:
            offset = self._parse_pmc_offset(out)
            with self._lock:
                self._status.offset_ms = offset
                self._status.last_sync_age_s = 0.0
                self._status.source = (TimeSyncSource.PTP_MASTER
                                       if self.is_master
                                       else TimeSyncSource.PTP_SLAVE)
                self._status.error_message = ""
            return
        with self._lock:
            self._status.last_sync_age_s += self.MONITOR_PERIOD_S
            if self._status.last_sync_age_s > self.STALE_THRESHOLD_S:
                self._status.source = TimeSyncSource.RTC_DRIFT
                self._status.error_message = "PTP daemon unreachable"

    @staticmethod
    def _parse_pmc_offset(text: str) -> float:
        """Extract `master_offset` (ns) from pmc output. Returns abs ms.
        Returns 999.0 when unparseable."""
        for line in text.splitlines():
            if "master_offset" not in line.lower():
                continue
            tokens = line.split()
            for i, t in enumerate(tokens):
                if "offset" in t.lower() and i + 1 < len(tokens):
                    try:
                        ns = int(tokens[i + 1])
                    except ValueError:
                        continue
                    return abs(ns) / 1_000_000.0
        return 999.0

    # ─── Public surface ───
    def get_status(self) -> TimeSyncStatus:
        with self._lock:
            return TimeSyncStatus(
                source=self._status.source,
                offset_ms=self._status.offset_ms,
                last_sync_age_s=self._status.last_sync_age_s,
                is_master=self._status.is_master,
                peers_synced=self._status.peers_synced,
                error_message=self._status.error_message,
            )

    def check_mission_ready(self) -> Tuple[bool, str]:
        """Mission start PreCheck — block if no PTP or offset > 5 ms."""
        s = self.get_status()
        if s.source <= TimeSyncSource.NTP:
            return False, f"PTP not active (source={s.source.name})"
        if s.offset_ms > self.PTP_MAX_OFFSET_MS:
            return False, (
                f"PTP offset {s.offset_ms:.2f}ms exceeds "
                f"{self.PTP_MAX_OFFSET_MS}ms limit")
        return True, f"PTP OK ({s.offset_ms:.2f}ms)"


def quorum_consensus_offset(peer_offsets_ms: list) -> float:
    """Median offset from peers — used as a fallback when the PTP
    master is unreachable (SDD §5.8.3).

    Returns 999.0 for an empty input so the call site can fold the
    \"no consensus\" branch into the same threshold logic as a
    missing-source state.
    """
    if not peer_offsets_ms:
        return 999.0
    sorted_offs = sorted(peer_offsets_ms)
    n = len(sorted_offs)
    if n % 2:
        return float(sorted_offs[n // 2])
    return (sorted_offs[n // 2 - 1] + sorted_offs[n // 2]) / 2.0
