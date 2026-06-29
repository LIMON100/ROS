#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
traffic_policy.py — Communication traffic relay policy for SkyHunter.

Pure Python — no ROS dependency.  Fully unit-testable in isolation.

Responsibilities:
  - Tier behavior matrix (what gets relayed at each tier)
  - Per-topic throttle tracking (should_relay)
  - Relay stats accumulation
"""

from dataclasses import dataclass
from typing import Dict


# ── Tier constants (mirrors comm_fsm.py) ─────────────────────────────────────

DISCONNECTED = 0
WIFI6        = 1
LTE          = 2
LORA         = 3


# ── Relay rates per tier per topic type ──────────────────────────────────────

# None  = pass-through (no throttle)
# 0.0   = blocked
# float = max relay rate in Hz

RELAY_RATES: Dict[int, Dict[str, float]] = {
    WIFI6: {
        "odom":   None,   # pass-through
        "camera": None,   # pass-through
        "cmd_vel": None,  # pass-through
    },
    LTE: {
        "odom":   2.0,    # throttle to 2 Hz
        "camera": 1.0,    # throttle to 1 fps
        "cmd_vel": None,  # pass-through (with 50ms delay handled by node)
    },
    LORA: {
        "odom":   0.0,    # blocked
        "camera": 0.0,    # blocked
        "cmd_vel": 0.0,   # blocked (e-stop only via lora_sim_stub)
    },
    DISCONNECTED: {
        "odom":   0.0,
        "camera": 0.0,
        "cmd_vel": 0.0,
    },
}


# ── Relay stats ───────────────────────────────────────────────────────────────

@dataclass
class RelayStats:
    sent:    int = 0
    dropped: int = 0
    bytes:   int = 0

    def record_sent(self, size_bytes: int = 0) -> None:
        self.sent += 1
        self.bytes += size_bytes

    def record_dropped(self) -> None:
        self.dropped += 1

    @property
    def total(self) -> int:
        return self.sent + self.dropped

    @property
    def drop_rate_pct(self) -> float:
        if self.total == 0:
            return 0.0
        return 100.0 * self.dropped / self.total


# ── Traffic policy ────────────────────────────────────────────────────────────

class TrafficPolicy:
    """
    Stateful relay policy.

    Tracks per-topic throttle timestamps and accumulated relay stats.
    Call should_relay() before publishing each message.
    """

    def __init__(self, num_robots: int) -> None:
        self.num_robots  = num_robots
        self.current_tier: int = WIFI6

        # last relay timestamps — key: "{topic_type}_{robot_id}", value: float (monotonic s)
        self._last_relay: Dict[str, float] = {}

        # per-robot-per-topic stats — key: "{topic_type}_{robot_id}"
        self._stats: Dict[str, RelayStats] = {}

    # ── Tier management ───────────────────────────────────────────────────────

    def set_tier(self, tier: int) -> None:
        self.current_tier = tier

    # ── Relay decision ────────────────────────────────────────────────────────

    def should_relay(
        self,
        topic_type: str,
        robot_id:   int,
        now_s:      float,
        msg_size_bytes: int = 0,
    ) -> bool:
        """
        Return True if this message should be relayed given the current tier.

        Updates throttle timestamps and relay stats automatically.

        Args:
            topic_type     : "odom" | "camera" | "cmd_vel"
            robot_id       : 1-8
            now_s          : monotonic clock in seconds
            msg_size_bytes : used for byte accounting in stats
        """
        key   = f"{topic_type}_{robot_id}"
        stats = self._stats.setdefault(key, RelayStats())

        rate = RELAY_RATES.get(self.current_tier, {}).get(topic_type, 0.0)

        # Blocked
        if rate == 0.0:
            stats.record_dropped()
            return False

        # Pass-through (no throttle)
        if rate is None:
            stats.record_sent(msg_size_bytes)
            return True

        # Throttled — check elapsed time
        last = self._last_relay.get(key, 0.0)
        if (now_s - last) >= (1.0 / rate):
            self._last_relay[key] = now_s
            stats.record_sent(msg_size_bytes)
            return True

        stats.record_dropped()
        return False

    # ── Stats access ──────────────────────────────────────────────────────────

    def get_stats(self, topic_type: str, robot_id: int) -> RelayStats:
        key = f"{topic_type}_{robot_id}"
        return self._stats.get(key, RelayStats())

    def get_total_stats(self) -> RelayStats:
        """Aggregate stats across all robots and topic types."""
        total = RelayStats()
        for s in self._stats.values():
            total.sent    += s.sent
            total.dropped += s.dropped
            total.bytes   += s.bytes
        return total

    def reset_stats(self) -> None:
        self._stats.clear()