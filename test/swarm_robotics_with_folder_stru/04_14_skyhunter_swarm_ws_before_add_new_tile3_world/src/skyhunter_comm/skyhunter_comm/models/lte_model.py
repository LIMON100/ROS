#!/usr/bin/env python3
"""
lte_model.py — LTE link state model for SkyHunter comm stack.

Pure Python — no ROS dependency.  Fully unit-testable in isolation.

Responsibilities:
  - LTE state constants (DISCONNECTED / CONNECTED / DEGRADED)
  - InjectedFailure dataclass
  - LteModel: applies active failures, computes RTT/loss/state each tick
"""

from dataclasses import dataclass, field
from random import uniform
from typing import List


# ── State constants ───────────────────────────────────────────────────────────

DISCONNECTED = 0
CONNECTED    = 1
DEGRADED     = 2

STATE_NAMES = {
    DISCONNECTED: "DISCONNECTED",
    CONNECTED:    "CONNECTED",
    DEGRADED:     "DEGRADED",
}

# Thresholds that trigger DEGRADED state
RTT_DEGRADE_THRESHOLD_MS   = 200.0   # ms
LOSS_DEGRADE_THRESHOLD_PCT = 20.0    # %

# RTT jitter range (keeps test results clean)
RTT_JITTER_MS = 5.0


# ── Injected failure dataclass ────────────────────────────────────────────────

@dataclass
class InjectedFailure:
    """
    Represents a single active failure injection.

    failure_type : "disconnect" | "latency" | "packet_loss"
    value        : extra ms (latency) or extra % (packet_loss); ignored for disconnect
    duration_s   : 0 = permanent until cleared; >0 = auto-expire
    start_s      : monotonic time when failure was injected
    """
    failure_type: str
    value:        float
    duration_s:   float
    start_s:      float


# ── LTE model parameters ──────────────────────────────────────────────────────

@dataclass
class LteModelConfig:
    base_rtt_ms:       float = 50.0
    base_loss_pct:     float = 2.0
    bandwidth_mbps:    float = 20.0
    auto_reconnect_s:  float = 15.0
    initial_state:     str   = "connected"


# ── LTE model ─────────────────────────────────────────────────────────────────

class LteModel:
    """
    Stateful LTE link simulator.

    Call tick(now_s) each cycle to get the current LteSnapshot.
    Call inject(failure) to add a failure.
    Call clear() to remove all active failures.
    """

    def __init__(self, config: LteModelConfig) -> None:
        self.config = config
        self._failures: List[InjectedFailure] = []
        self._start_s: float = 0.0       # set on first tick
        self._initialized: bool = False

    # ── Public interface ──────────────────────────────────────────────────────

    def inject(self, failure: InjectedFailure) -> None:
        """Add a failure injection."""
        self._failures.append(failure)

    def clear(self) -> None:
        """Remove all active failures."""
        self._failures.clear()

    def tick(self, now_s: float) -> "LteSnapshot":
        """
        Compute current LTE state.  Call at the desired publish rate.

        Returns an LteSnapshot with all fields populated.
        """
        if not self._initialized:
            self._start_s = now_s
            self._initialized = True

        uptime_s = now_s - self._start_s

        # ── Expire timed failures ─────────────────────────────────────────────
        self._failures = [
            f for f in self._failures
            if not (f.duration_s > 0 and (now_s - f.start_s) >= f.duration_s)
        ]

        # ── Apply active failures ─────────────────────────────────────────────
        rtt_ms   = self.config.base_rtt_ms
        loss_pct = self.config.base_loss_pct
        state    = CONNECTED

        for f in self._failures:
            if f.failure_type == "disconnect":
                state = DISCONNECTED
            elif f.failure_type == "latency":
                rtt_ms += f.value
            elif f.failure_type == "packet_loss":
                loss_pct += f.value

        # ── Degrade state if metrics are bad (but not fully disconnected) ─────
        if state == CONNECTED:
            if rtt_ms > RTT_DEGRADE_THRESHOLD_MS or loss_pct > LOSS_DEGRADE_THRESHOLD_PCT:
                state = DEGRADED

        # ── Add small RTT jitter ──────────────────────────────────────────────
        rtt_ms += uniform(-RTT_JITTER_MS, RTT_JITTER_MS)
        rtt_ms  = max(0.0, rtt_ms)

        return LteSnapshot(
            state          = state,
            rtt_ms         = rtt_ms,
            packet_loss_pct= loss_pct,
            bandwidth_mbps = self.config.bandwidth_mbps,
            uptime_s       = uptime_s,
        )

    @property
    def active_failure_count(self) -> int:
        return len(self._failures)


# ── Snapshot (plain data — no ROS types) ─────────────────────────────────────

@dataclass
class LteSnapshot:
    """
    Plain data snapshot of LTE state at one tick.
    The ROS node maps this directly onto LteStatus.msg fields.
    """
    state:           int
    rtt_ms:          float
    packet_loss_pct: float
    bandwidth_mbps:  float
    uptime_s:        float