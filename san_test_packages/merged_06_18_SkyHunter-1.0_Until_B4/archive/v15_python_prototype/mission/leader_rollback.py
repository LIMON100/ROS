"""LeaderRollbackChecker — retreat when ≥ 50 % followers are struggling.

SDD Rev.A.6 §7.6 + user decision #6.

State machine:
  NORMAL → ROLLBACK_INITIATED  (ratio ≥ 0.5)
         → RETREATING          (next tick)
         → REPLANNING          (after RETREAT_DURATION_S)
         → NORMAL (success) | NO_VIABLE_PATH (fail, via replan_complete)

Hysteresis: any non-NORMAL state with ratio < THRESHOLD_RECOVERY (0.3)
returns to NORMAL — followers caught up enough to drop the rollback.

Pure logic — no IPC, no threading. Caller drives once per second with a
fresh follower-tier snapshot.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Optional

from swarm.tier_manager import Tier, TierManager


class RollbackState(IntEnum):
    NORMAL = 0
    ROLLBACK_INITIATED = 1
    RETREATING = 2
    REPLANNING = 3
    NO_VIABLE_PATH = 4


@dataclass
class FollowerStatus:
    follower_id: int
    tier: Tier
    last_update_ts: float = 0.0


@dataclass
class StablePosition:
    """Recorded position when the swarm was last fully T0/T1."""
    x: float
    y: float
    heading_rad: float
    timestamp: float


@dataclass
class RollbackEvent:
    state: RollbackState
    n_struggling: int
    n_total: int
    ratio: float
    message: str
    avoid_corridors: List[str] = field(default_factory=list)


class LeaderRollbackChecker:
    """Run on the leader at 1 Hz to monitor swarm health."""

    THRESHOLD_INITIATE = 0.5      # ≥ 50 % struggling
    THRESHOLD_RECOVERY = 0.3      # < 30 % to drop back to NORMAL
    RETREAT_DURATION_S = 30.0
    STABLE_HISTORY_LEN = 60       # ≈ last minute @ 1 Hz

    def __init__(self):
        self.state = RollbackState.NORMAL
        self._stable_history: List[StablePosition] = []
        self._rollback_start_ts: Optional[float] = None

    # ─── State ingest ───
    def update_followers(self, followers: List[FollowerStatus]
                         ) -> Optional[RollbackEvent]:
        """Update once per second with all current follower tiers.

        Returns a RollbackEvent only on a state-machine transition.
        """
        if not followers:
            return None
        n_total = len(followers)
        n_struggling = sum(1 for f in followers
                           if TierManager.is_struggling(f.tier))
        ratio = n_struggling / n_total

        prev_state = self.state

        if self.state == RollbackState.NORMAL:
            if ratio >= self.THRESHOLD_INITIATE:
                self.state = RollbackState.ROLLBACK_INITIATED
                self._rollback_start_ts = time.monotonic()
                return self._event(n_struggling, n_total, ratio,
                                   f"ROLLBACK_INITIATED ({n_struggling}/{n_total})")

        elif self.state == RollbackState.ROLLBACK_INITIATED:
            self.state = RollbackState.RETREATING
            # Note: no event on this internal step — caller saw the
            # initiation already.

        elif self.state == RollbackState.RETREATING:
            if (self._rollback_start_ts is not None and
                    time.monotonic() - self._rollback_start_ts
                    >= self.RETREAT_DURATION_S):
                self.state = RollbackState.REPLANNING
                return self._event(n_struggling, n_total, ratio,
                                   "REPLANNING — retreat complete")

        # REPLANNING: external call to replan_complete() advances state.
        # NO_VIABLE_PATH: terminal until operator intervenes.

        # Hysteresis recovery — applies to all non-NORMAL states.
        if (self.state != RollbackState.NORMAL
                and self.state != prev_state):
            # Just transitioned; let caller see this event before checking.
            pass
        if (self.state != RollbackState.NORMAL
                and ratio < self.THRESHOLD_RECOVERY):
            self.state = RollbackState.NORMAL
            self._rollback_start_ts = None
            return self._event(n_struggling, n_total, ratio,
                               "ROLLBACK_RECOVERY — followers caught up")
        return None

    # ─── Auxiliary ───
    def record_stable_position(self, x: float, y: float,
                               heading: float) -> None:
        """Record current pose when the entire swarm is T0/T1."""
        self._stable_history.append(StablePosition(
            x=x, y=y, heading_rad=heading,
            timestamp=time.monotonic()))
        if len(self._stable_history) > self.STABLE_HISTORY_LEN:
            self._stable_history = self._stable_history[
                -self.STABLE_HISTORY_LEN:]

    def get_retreat_target(self) -> Optional[StablePosition]:
        """Last stable position recorded before rollback initiation."""
        if self._rollback_start_ts is None or not self._stable_history:
            return None
        for sp in reversed(self._stable_history):
            if sp.timestamp <= self._rollback_start_ts:
                return sp
        return self._stable_history[0]

    def replan_complete(self, success: bool) -> None:
        """Caller advances REPLANNING after attempting to plan around
        the avoid-corridor list."""
        if self.state != RollbackState.REPLANNING:
            return
        if success:
            self.state = RollbackState.NORMAL
            self._rollback_start_ts = None
        else:
            self.state = RollbackState.NO_VIABLE_PATH

    # ─── Internal ───
    def _event(self, n_struggling: int, n_total: int, ratio: float,
               message: str) -> RollbackEvent:
        return RollbackEvent(
            state=self.state, n_struggling=n_struggling,
            n_total=n_total, ratio=ratio, message=message,
        )
