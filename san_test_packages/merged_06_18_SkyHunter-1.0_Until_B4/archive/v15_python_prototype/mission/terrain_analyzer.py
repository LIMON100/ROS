"""Auto terrain analyzer for formation switch (SDD §7.5, P2-3).

4 trigger conditions for column auto-switch:
1. LiDAR passable width < D_across + 2 m safety margin
2. Cost map untraversable cells within 0.5 m on both sides laterally
3. DEM elevation change > 1 m within 5 m laterally
4. Tier 4 follower ratio ≥ 1/3

5-second operator confirmation window before auto-switch.
Auto-revert to previous formation when terrain clears (10s hysteresis).
"""
from __future__ import annotations

import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, List, Optional, Tuple

from swarm.tier_manager import Tier


class TerrainState(IntEnum):
    OPEN = 0            # current formation can continue
    NARROW_PENDING = 1  # trigger fired, awaiting confirm
    NARROW_ACTIVE = 2   # column auto-switch active
    CLEARING = 3        # terrain opening, awaiting hysteresis


@dataclass
class TerrainTriggers:
    lidar_narrow: bool = False
    costmap_blocked_lateral: bool = False
    dem_elevation_change: bool = False
    tier4_ratio: bool = False
    measured_width_m: float = 999.0
    measured_dem_change_m: float = 0.0
    measured_tier4_ratio: float = 0.0

    def any_triggered(self) -> bool:
        return (self.lidar_narrow or self.costmap_blocked_lateral
                or self.dem_elevation_change or self.tier4_ratio)

    def all_clear(self) -> bool:
        return not self.any_triggered()


@dataclass
class TerrainEvent:
    state: TerrainState
    triggers: TerrainTriggers
    seconds_until_action: float = 0.0
    message: str = ""


class TerrainAnalyzer:
    """Run 1 Hz on Leader. Recommends formation transition."""

    CONFIRM_WINDOW_S = 5.0
    REVERT_HYSTERESIS_S = 10.0
    LIDAR_SAFETY_MARGIN_M = 2.0
    COSTMAP_LATERAL_CHECK_M = 0.5
    DEM_LATERAL_CHECK_M = 5.0
    DEM_ELEVATION_THRESHOLD_M = 1.0
    TIER4_RATIO_THRESHOLD = 1.0 / 3.0
    COSTMAP_BLOCK_THRESHOLD = 0.95

    def __init__(self) -> None:
        self.state: TerrainState = TerrainState.OPEN
        self._pending_since: Optional[float] = None
        self._clearing_since: Optional[float] = None
        self.previous_formation: Optional[str] = None
        self._auto_switch_callback: Optional[Callable[[bool], None]] = None

    def set_auto_switch_callback(self, fn: Callable[[bool], None]) -> None:
        """fn(switch_to_column: bool) called when state changes commit."""
        self._auto_switch_callback = fn

    def evaluate(self,
                 lidar_passable_width_m: float,
                 costmap_lateral_costs: Tuple[float, float],
                 dem_elevation_diff_m: float,
                 follower_tiers: List[Tier],
                 current_formation_d_m: float = 5.0) -> TerrainEvent:
        triggers = self._compute_triggers(
            lidar_passable_width_m,
            costmap_lateral_costs,
            dem_elevation_diff_m,
            follower_tiers,
            current_formation_d_m,
        )
        msg = ""
        secs = 0.0
        now = time.monotonic()

        if self.state == TerrainState.OPEN:
            if triggers.any_triggered():
                self.state = TerrainState.NARROW_PENDING
                self._pending_since = now
                self._clearing_since = None
                msg = "terrain trigger — switching to column in 5s"
                secs = self.CONFIRM_WINDOW_S

        elif self.state == TerrainState.NARROW_PENDING:
            elapsed = now - (self._pending_since or now)
            secs = max(0.0, self.CONFIRM_WINDOW_S - elapsed)
            if triggers.all_clear():
                self.state = TerrainState.OPEN
                self._pending_since = None
                msg = "trigger cleared during pending — staying open"
            elif elapsed >= self.CONFIRM_WINDOW_S:
                self.state = TerrainState.NARROW_ACTIVE
                self._pending_since = None
                if self._auto_switch_callback:
                    self._auto_switch_callback(True)
                msg = "auto-switched to column"
            else:
                msg = f"awaiting confirm ({secs:.0f}s)"

        elif self.state == TerrainState.NARROW_ACTIVE:
            if triggers.all_clear():
                self.state = TerrainState.CLEARING
                self._clearing_since = now
                msg = "terrain clearing — reverting in 10s"

        elif self.state == TerrainState.CLEARING:
            elapsed = now - (self._clearing_since or now)
            secs = max(0.0, self.REVERT_HYSTERESIS_S - elapsed)
            if triggers.any_triggered():
                self.state = TerrainState.NARROW_ACTIVE
                self._clearing_since = None
                msg = "trigger fired again — staying in column"
            elif elapsed >= self.REVERT_HYSTERESIS_S:
                self.state = TerrainState.OPEN
                self._clearing_since = None
                if self._auto_switch_callback:
                    self._auto_switch_callback(False)
                msg = "reverted to previous formation"
            else:
                msg = f"reverting in {secs:.0f}s"

        return TerrainEvent(
            state=self.state,
            triggers=triggers,
            seconds_until_action=secs,
            message=msg,
        )

    def _compute_triggers(self,
                          lidar_w: float,
                          costmap_lateral: Tuple[float, float],
                          dem_diff: float,
                          tiers: List[Tier],
                          current_d: float) -> TerrainTriggers:
        # Tier values may arrive either as the IntEnum (`Tier.T4`) when
        # callers consume `tier_manager.TierUpdate` directly, or as the
        # string `"T4"` when forwarded over the `sw_tier` DDS topic
        # (see `core/messages.TIER_T4`). Accept both so the trigger
        # actually fires in production.
        n_t4 = sum(1 for t in tiers if t == Tier.T4 or t == "T4")
        ratio = n_t4 / len(tiers) if tiers else 0.0
        d_across = current_d * 1.5  # heuristic for v_shape
        return TerrainTriggers(
            lidar_narrow=lidar_w < (d_across + self.LIDAR_SAFETY_MARGIN_M),
            costmap_blocked_lateral=(
                max(costmap_lateral) > self.COSTMAP_BLOCK_THRESHOLD
                if costmap_lateral else False),
            dem_elevation_change=(
                abs(dem_diff) > self.DEM_ELEVATION_THRESHOLD_M),
            tier4_ratio=ratio >= self.TIER4_RATIO_THRESHOLD,
            measured_width_m=lidar_w,
            measured_dem_change_m=dem_diff,
            measured_tier4_ratio=ratio,
        )

    def manual_override(self, accept: bool) -> None:
        """Operator confirms (accept=True) or cancels (accept=False)."""
        if self.state != TerrainState.NARROW_PENDING:
            return
        if accept:
            self.state = TerrainState.NARROW_ACTIVE
            self._pending_since = None
            if self._auto_switch_callback:
                self._auto_switch_callback(True)
        else:
            self.state = TerrainState.OPEN
            self._pending_since = None
