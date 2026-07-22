"""Battery 20 % auto RTH trigger (SDD Rev.A.6 §8).

Three thresholds, one direction (declining SoC):

  < 30 %  → WARN       (operator alert only, mission continues)
  ≤ 20 %  → RTH        (mission abort + Return-To-Home behavior tree)
  ≤ 10 %  → EMERGENCY  (immediate Stand mode + alert)

Hysteresis (2 % default) prevents thrashing during slow charging:
once RTH fires at 20 %, the action only clears once SoC reaches
22 % or higher.

dev_override (PIN-authenticated only) suppresses every action — used
on the bench so a developer can drain a battery to test edge cases
without the robot running off to find its dock.
"""
from __future__ import annotations

from enum import IntEnum
from typing import Callable, Optional


class BatteryAction(IntEnum):
    NONE = 0
    WARN = 1
    RTH = 2
    EMERGENCY = 3


class BatteryMonitor:

    WARN_THRESHOLD = 30.0
    RTH_THRESHOLD = 20.0
    EMERGENCY_THRESHOLD = 10.0
    HYSTERESIS_PCT = 2.0

    def __init__(self,
                 dev_override: bool = False,
                 on_action: Optional[Callable[[BatteryAction, float],
                                              None]] = None):
        self.dev_override = bool(dev_override)
        self.on_action = on_action
        self._current_action: BatteryAction = BatteryAction.NONE
        self._last_soc: Optional[float] = None

    # ─── Tick ───
    def update(self, soc_pct: float) -> BatteryAction:
        """Called at 1 Hz. Returns the action triggered, or NONE if no
        state change. The on_action callback also fires on transitions."""
        if self.dev_override:
            return BatteryAction.NONE

        new_action = self._compute_action(soc_pct)

        # Hysteresis on recovery: once we've armed an action, demand the
        # SoC come back above the threshold + margin before we drop it.
        if new_action < self._current_action:
            margins = {
                BatteryAction.EMERGENCY:
                    self.EMERGENCY_THRESHOLD + self.HYSTERESIS_PCT,
                BatteryAction.RTH:
                    self.RTH_THRESHOLD + self.HYSTERESIS_PCT,
                BatteryAction.WARN:
                    self.WARN_THRESHOLD + self.HYSTERESIS_PCT,
            }
            margin = margins.get(self._current_action)
            if margin is not None and soc_pct < margin:
                self._last_soc = soc_pct
                return BatteryAction.NONE

        self._last_soc = soc_pct
        if new_action != self._current_action:
            self._current_action = new_action
            if self.on_action is not None:
                self.on_action(new_action, soc_pct)
            return new_action
        return BatteryAction.NONE

    # ─── State ───
    @property
    def current_action(self) -> BatteryAction:
        return self._current_action

    # ─── Internal ───
    def _compute_action(self, soc_pct: float) -> BatteryAction:
        if soc_pct <= self.EMERGENCY_THRESHOLD:
            return BatteryAction.EMERGENCY
        if soc_pct <= self.RTH_THRESHOLD:
            return BatteryAction.RTH
        if soc_pct < self.WARN_THRESHOLD:
            return BatteryAction.WARN
        return BatteryAction.NONE
