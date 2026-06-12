"""Dual-SBC peer heartbeat surveillance for the Hub UGV.

The Hub UGV runs on two SBCs (SBC#1 = SLAM, SBC#2 = Comm). Each SBC
publishes a heartbeat the other watches; if the heartbeat is silent
past HEARTBEAT_TIMEOUT_SEC, the surviving SBC fires a ThreatAlert so
the swarm can fall back to partial-operation mode (SLAM-down → other
robots assist with comm routing; Comm-down → operator UI flags the
loss but local SLAM keeps running).

This module is pure logic: `record_heartbeat` is called from whichever
DDS / IPC bridge ingests peer heartbeats, and `check_timeouts` is
called on a tick from the surviving SBC's main loop. Both methods are
thread-safe so the heartbeat ingest and the tick can run on separate
threads in HubUgvAdapter.
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence

HEARTBEAT_TIMEOUT_SEC = 3.0


@dataclass
class _PeerState:
    last_seen_t: float = 0.0
    timed_out: bool = False     # latched until check_recoveries sees a fresh beat


class HubHealthMonitor:
    """Track peer SBC heartbeats; surface fresh timeouts + recoveries."""

    def __init__(
        self,
        peer_ids: Sequence[str],
        timeout_sec: float = HEARTBEAT_TIMEOUT_SEC,
    ):
        self.timeout_sec = float(timeout_sec)
        # Pre-populated: every configured peer starts as "never seen", so
        # the first check_timeouts() call AFTER `timeout_sec` flags any
        # peer that never beat at all (catches a never-boot peer).
        self._peers: Dict[str, _PeerState] = {
            str(pid): _PeerState() for pid in peer_ids
        }
        self._lock = threading.Lock()

    # ─── Ingest ────────────────────────────────────────────────────────

    def record_heartbeat(self, peer_id: str, now: Optional[float] = None) -> bool:
        """Pulse from a peer. Returns True if this beat recovers a
        previously-timed-out peer (so callers can publish a clear event).

        Unknown peer_ids are silently ignored — they don't get added to
        the roster mid-flight; configure them up front instead.
        """
        n = now if now is not None else time.monotonic()
        with self._lock:
            p = self._peers.get(str(peer_id))
            if p is None:
                return False
            recovered = p.timed_out
            p.last_seen_t = n
            p.timed_out = False
            return recovered

    # ─── Tick ──────────────────────────────────────────────────────────

    def check_timeouts(self, now: Optional[float] = None) -> List[str]:
        """Return the list of peers that have JUST gone stale (de-duped).

        A peer is only listed once per outage; it won't reappear until
        a fresh heartbeat lands and check_timeouts catches the next
        timeout cycle. The order is sorted for deterministic output.
        """
        n = now if now is not None else time.monotonic()
        stale: List[str] = []
        with self._lock:
            for pid, p in self._peers.items():
                if p.timed_out:
                    continue
                age = n - p.last_seen_t if p.last_seen_t > 0 else float("inf")
                if age > self.timeout_sec:
                    p.timed_out = True
                    stale.append(pid)
        return sorted(stale)

    # ─── Introspection ─────────────────────────────────────────────────

    def is_timed_out(self, peer_id: str) -> bool:
        with self._lock:
            p = self._peers.get(str(peer_id))
            return bool(p and p.timed_out)

    def peer_ids(self) -> List[str]:
        with self._lock:
            return list(self._peers.keys())


__all__ = (
    "HEARTBEAT_TIMEOUT_SEC",
    "HubHealthMonitor",
)
