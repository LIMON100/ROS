"""3-strike cost-map avoidance failure detector (SAN v1.3 §6.4).

Pure compute, no IPC. Caller drives:

    alarm = CostMapAvoidanceAlarm(max_failures=3)
    on each Tier 1.5 reroute attempt:
        alarm.record(success: bool, reason: str = "")
        alert = alarm.poll_alert()      # OperatorAlert or None
        if alert is not None:
            publish(queues.operator_alert, alert)

Hysteresis: a single success resets the consecutive-failure counter.
The alarm fires once per `max_failures`-strike streak (it won't spam
the operator if the failures persist — they re-fire only after a
success-then-fail transition).
"""
from __future__ import annotations

import time
from typing import Optional

from core.messages import Header, OperatorAlert

_DEFAULT_MAX_FAILURES = 3


class CostMapAvoidanceAlarm:
    """Counts consecutive Tier 1.5 reroute failures, emits an alert once
    the streak hits the configured threshold.
    """

    def __init__(self, max_failures: int = _DEFAULT_MAX_FAILURES):
        if max_failures < 1:
            raise ValueError("max_failures must be ≥ 1")
        self.max_failures = int(max_failures)
        self._consecutive: int = 0
        self._last_reason: str = ""
        self._pending: Optional[OperatorAlert] = None
        # Latch: once we've emitted for a streak we suppress further
        # alerts until the streak resets (a single success).
        self._latched: bool = False
        # Stats — exposed for tests + the debug dashboard.
        self.total_failures: int = 0
        self.total_alerts: int = 0

    def record(self, success: bool, reason: str = "") -> None:
        """Tick the counter. `success=True` resets the streak."""
        if success:
            self._consecutive = 0
            self._latched = False
            return
        self._consecutive += 1
        self.total_failures += 1
        self._last_reason = reason or self._last_reason
        if self._consecutive >= self.max_failures and not self._latched:
            self._pending = OperatorAlert(
                header=Header(stamp=time.monotonic()),
                code="cost_map_avoidance_failed",
                description=(
                    f"Tier 1.5 reroute failed {self._consecutive}× in a row "
                    f"(last reason: {self._last_reason or 'unknown'})"),
                severity="warning",
                detail={
                    "consecutive_failures": self._consecutive,
                    "last_reason": self._last_reason,
                },
            )
            self._latched = True
            self.total_alerts += 1

    def poll_alert(self) -> Optional[OperatorAlert]:
        """Return + clear the pending alert (or None if no alert)."""
        a = self._pending
        self._pending = None
        return a

    @property
    def consecutive_failures(self) -> int:
        return self._consecutive

    def reset(self) -> None:
        self._consecutive = 0
        self._latched = False
        self._pending = None
