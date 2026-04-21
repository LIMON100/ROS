"""
comm_fsm.py — Communication tier FSM for SkyHunter swarm.

Pure Python — no ROS dependency.  Fully unit-testable in isolation.

Usage:
    fsm = CommFSM(params)
    fsm.update(wifi6_ok, wifi6_recovered, lte_ok, lora_ok, now_s=time.monotonic())
    print(fsm.current_tier)
"""

import logging
from dataclasses import dataclass, field
from typing import Optional

logger = logging.getLogger(__name__)


# ── Tier constants ────────────────────────────────────────────────────────────

DISCONNECTED = 0
WIFI6        = 1
LTE          = 2
LORA         = 3

TIER_NAMES = {
    DISCONNECTED: "DISCONNECTED",
    WIFI6:        "WIFI6",
    LTE:          "LTE",
    LORA:         "LORA",
}


# ── FSM parameters dataclass ──────────────────────────────────────────────────

@dataclass
class CommFSMParams:
    wifi6_fail_duration_s:    float = 3.0
    wifi6_recovery_duration_s: float = 5.0
    lte_fail_duration_s:      float = 10.0


# ── Hysteresis timer ──────────────────────────────────────────────────────────

@dataclass
class HysteresisTimer:
    """Tracks how long a condition has been sustained."""
    start_s: Optional[float] = field(default=None)

    def arm(self, now_s: float) -> None:
        """Start the timer if not already running."""
        if self.start_s is None:
            self.start_s = now_s

    def elapsed(self, now_s: float) -> float:
        """Seconds since timer was armed.  Returns 0 if not armed."""
        if self.start_s is None:
            return 0.0
        return now_s - self.start_s

    def sustained(self, now_s: float, required_s: float) -> bool:
        """True once condition has been sustained for required_s seconds."""
        return self.elapsed(now_s) >= required_s

    def reset(self) -> None:
        self.start_s = None


# ── FSM ───────────────────────────────────────────────────────────────────────

class CommFSM:
    """
    3-tier communication failover FSM.

    Tier priority (high → low): WIFI6 → LTE → LORA → DISCONNECTED

    Transition rules:
      WIFI6:        → LTE/LORA/DISCONNECTED  if WiFi6 bad for wifi6_fail_duration_s
      LTE:          → WIFI6                  if WiFi6 recovered for wifi6_recovery_duration_s
                    → LORA/DISCONNECTED      if LTE bad for lte_fail_duration_s
      LORA:         → WIFI6                  if WiFi6 recovered (with hysteresis)
                    → LTE                    immediately if LTE recovers
                    → DISCONNECTED           if LoRa lost
      DISCONNECTED: → best available tier    immediately
    """

    def __init__(self, params: CommFSMParams) -> None:
        self.params = params

        self.current_tier:  int = WIFI6
        self.previous_tier: int = WIFI6
        self.time_in_tier_s: float = 0.0   # updated each call

        # Hysteresis timers
        self._wifi6_fail    = HysteresisTimer()
        self._wifi6_recover = HysteresisTimer()
        self._lte_fail      = HysteresisTimer()

        self._tier_start_s: Optional[float] = None  # set on first update()

    # ── Public interface ──────────────────────────────────────────────────────

    def update(
        self,
        wifi6_ok:        bool,
        wifi6_recovered: bool,
        lte_ok:          bool,
        lora_ok:         bool,
        now_s:           float,
    ) -> bool:
        """
        Run one FSM cycle.

        Args:
            wifi6_ok:        WiFi6 meets operational thresholds (fail threshold).
            wifi6_recovered: WiFi6 meets recovery thresholds (stricter, for hysteresis).
            lte_ok:          LTE is healthy.
            lora_ok:         LoRa is alive.
            now_s:           Monotonic clock in seconds.

        Returns:
            True if a tier transition occurred this cycle.
        """
        if self._tier_start_s is None:
            self._tier_start_s = now_s

        self.time_in_tier_s = now_s - self._tier_start_s

        # ── Advance or reset hysteresis timers ────────────────────────────────
        self._tick_timers(wifi6_ok, wifi6_recovered, lte_ok, now_s)

        # ── Evaluate transitions ──────────────────────────────────────────────
        new_tier = self._evaluate(wifi6_ok, wifi6_recovered, lte_ok, lora_ok, now_s)

        if new_tier != self.current_tier:
            self._transition(new_tier, now_s)
            return True
        return False

    # ── Internal helpers ──────────────────────────────────────────────────────

    def _tick_timers(
        self,
        wifi6_ok: bool,
        wifi6_recovered: bool,
        lte_ok: bool,
        now_s: float,
    ) -> None:
        # WiFi6 fail timer — arm while bad, reset when good
        if not wifi6_ok:
            self._wifi6_fail.arm(now_s)
        else:
            self._wifi6_fail.reset()

        # WiFi6 recovery timer — arm while recovered, reset when not
        if wifi6_recovered:
            self._wifi6_recover.arm(now_s)
        else:
            self._wifi6_recover.reset()

        # LTE fail timer — arm while bad, reset when good
        if not lte_ok:
            self._lte_fail.arm(now_s)
        else:
            self._lte_fail.reset()

    def _evaluate(
        self,
        wifi6_ok:        bool,
        wifi6_recovered: bool,
        lte_ok:          bool,
        lora_ok:         bool,
        now_s:           float,
    ) -> int:
        p = self.params

        if self.current_tier == WIFI6:
            if not wifi6_ok and self._wifi6_fail.sustained(now_s, p.wifi6_fail_duration_s):
                return self._best_fallback(lte_ok, lora_ok)

        elif self.current_tier == LTE:
            if wifi6_recovered and self._wifi6_recover.sustained(now_s, p.wifi6_recovery_duration_s):
                return WIFI6
            if not lte_ok and self._lte_fail.sustained(now_s, p.lte_fail_duration_s):
                return LORA if lora_ok else DISCONNECTED

        elif self.current_tier == LORA:
            if wifi6_recovered and self._wifi6_recover.sustained(now_s, p.wifi6_recovery_duration_s):
                return WIFI6
            if lte_ok:
                return LTE
            if not lora_ok:
                return DISCONNECTED

        elif self.current_tier == DISCONNECTED:
            if wifi6_recovered:
                return WIFI6
            if lte_ok:
                return LTE
            if lora_ok:
                return LORA

        return self.current_tier  # no change

    def _best_fallback(self, lte_ok: bool, lora_ok: bool) -> int:
        if lte_ok:
            return LTE
        if lora_ok:
            return LORA
        return DISCONNECTED

    def _transition(self, new_tier: int, now_s: float) -> None:
        old_name = TIER_NAMES[self.current_tier]
        new_name = TIER_NAMES[new_tier]
        logger.warning("Tier transition: %s → %s", old_name, new_name)

        self.previous_tier = self.current_tier
        self.current_tier  = new_tier
        self._tier_start_s = now_s

        # Reset all hysteresis timers on any transition
        self._wifi6_fail.reset()
        self._wifi6_recover.reset()
        self._lte_fail.reset()