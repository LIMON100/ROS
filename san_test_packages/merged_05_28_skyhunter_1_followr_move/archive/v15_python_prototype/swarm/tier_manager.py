"""
TierManager — 5-Tier escape FSM for one follower (SDD Rev.A.5 §6.7).

Per-tier semantics:
  T0    Predictive track — fresh `FollowerTargetMessage` valid; follow it
        directly. Best case, no error correction needed.
  T1.5  Auto-reroute — local Nav2 has detected an obstacle and reroutes
        with ≤ 2 m offset from the leader's intended path. Still in
        formation; just nudged.
  T1    Normal PID — no fresh target, but along-path offset δ < 1.2·d₀.
        Standard formation-following.
  T2    Catch-up — 1.2·d₀ ≤ δ < 1.5·d₀. Boost speed within safety bounds.
  T3    Fast catch-up — 1.5·d₀ ≤ δ < 2.0·d₀. Aggressive cruise.
  T4    Breadcrumb recovery — δ ≥ 2.0·d₀. Drop formation; follow
        breadcrumb trail to the leader.

Hysteresis (anti-chattering):
  • T2 → T1   only when δ ≤ 1.2·d₀ * 0.95   (i.e. comfortably below the
                                              entry threshold)
  • T3 → T2   when δ ≤ 1.5·d₀
  • T4 → T2   when δ ≤ 2.0·d₀ (skip T3 — we want a clean re-entry)

The manager is pure logic — no IPC, no threading. Caller drives it by
invoking `update(delta_m, has_fresh_target, reroute_active)` once per
control cycle.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

from core.messages import (
    TIER_T0,
    TIER_T1,
    TIER_T1_5,
    TIER_T2,
    TIER_T3,
    TIER_T4,
)


class Tier(IntEnum):
    """5-tier escape ladder values (P1-4 leader rollback consumer API).

    Distinct from the core.messages.TIER_T* string constants the legacy
    TierManager state machine uses on the sw_tier topic payload — both
    are kept so callers can use whichever they prefer. is_struggling()
    accepts either form.
    """
    T0 = 0
    T1_5 = 1
    T1 = 2
    T2 = 3
    T3 = 4
    T4 = 5

# Entry thresholds (multiples of d₀, the nominal spacing).
_T2_ENTRY = 1.2
_T3_ENTRY = 1.5
_T4_ENTRY = 2.0
# Hysteresis exit margins per SDD §6.7 Table.
_T2_EXIT = 1.2     # T2 → T1 when δ/d₀ ≤ 1.2 * 0.95 (5 % deadband)
_T3_EXIT = 1.5     # T3 → T2 when δ/d₀ ≤ 1.5
_T4_EXIT = 2.0     # T4 → T2 when δ/d₀ ≤ 2.0 (skip T3)
_T2_DEADBAND = 0.05


@dataclass(slots=True)
class TierUpdate:
    """Snapshot returned by TierManager.update()."""
    prev: str
    cur: str
    delta_m: float
    d0_m: float
    transitioned: bool


class TierManager:
    """5-Tier FSM for one follower.

    Args:
      d0_m: nominal along-path follower spacing (m). Used to scale the
            entry / exit thresholds.
    """

    def __init__(self, *, d0_m: float = 3.0):
        self.d0_m = float(d0_m)
        self._tier: str = TIER_T1
        self._stats = {"transitions": 0}

    @property
    def tier(self) -> str:
        return self._tier

    @property
    def transitions(self) -> int:
        return self._stats["transitions"]

    def update(
        self,
        *,
        delta_m: float,
        has_fresh_target: bool = False,
        reroute_active: bool = False,
    ) -> TierUpdate:
        """Tick the FSM. Order of precedence:

        1. T0 fires when a fresh leader-derived target is available AND
           we're inside the formation envelope (δ < 1.2·d₀).
        2. T1.5 fires when the local planner is rerouting around an
           obstacle but within 2·d₀ of the formation path (i.e. the
           ratio gate is the same as T4 entry — past that, it's a real
           escape, not a planner nudge).
        3. Otherwise we use the δ-driven ladder with hysteresis.
        """
        prev = self._tier
        ratio = delta_m / max(self.d0_m, 1e-3)

        new = prev
        if has_fresh_target and ratio < _T2_ENTRY:
            new = TIER_T0
        elif reroute_active and ratio < _T4_ENTRY:
            new = TIER_T1_5
        else:
            # Hysteresis-aware ladder
            new = self._step_ratio(prev, ratio)

        if new != prev:
            self._stats["transitions"] += 1
            self._tier = new
        return TierUpdate(prev=prev, cur=new, delta_m=delta_m,
                           d0_m=self.d0_m,
                           transitioned=(new != prev))

    @staticmethod
    def is_struggling(tier) -> bool:
        """T3+ counts as struggling. Accepts Tier enum or TIER_T* string."""
        if isinstance(tier, Tier):
            return tier in (Tier.T3, Tier.T4)
        return tier in (TIER_T3, TIER_T4)

    @staticmethod
    def _step_ratio(prev: str, ratio: float) -> str:
        """Pure ratio-only ladder with hysteresis on the way down."""
        # First, climb up if we're past an entry threshold.
        if ratio >= _T4_ENTRY:
            return TIER_T4
        if ratio >= _T3_ENTRY:
            # T4 → T3 not allowed (we go straight T4 → T2 per spec)
            if prev == TIER_T4:
                return TIER_T4 if ratio >= _T4_EXIT else TIER_T2
            return TIER_T3
        if ratio >= _T2_ENTRY:
            return TIER_T2
        # Below T2 entry → consider exit transitions.
        if prev == TIER_T4 and ratio >= _T4_EXIT:
            return TIER_T4
        if prev == TIER_T4:        # ratio < 2.0 → T4 → T2 per spec
            return TIER_T2
        if prev == TIER_T3 and ratio >= _T3_EXIT:
            return TIER_T3
        if prev == TIER_T3:        # ratio < 1.5 → T3 → T2
            return TIER_T2
        if prev == TIER_T2:
            # Need to fall by deadband below the entry threshold to drop
            # back into T1, otherwise we'd chatter near δ = 1.2·d₀.
            if ratio <= _T2_EXIT * (1.0 - _T2_DEADBAND):
                return TIER_T1
            return TIER_T2
        return TIER_T1
