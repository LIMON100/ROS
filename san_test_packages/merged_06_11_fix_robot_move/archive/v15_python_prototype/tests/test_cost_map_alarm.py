"""3-strike avoidance failure alarm (SAN v1.3 §6.4).

Verifies the operator_alert emission policy:
  • 2 failures in a row → no alert yet
  • 3 failures in a row → exactly one alert fired
  • additional failures → no further alerts (latch)
  • a success resets the streak; subsequent 3 failures fire again
"""
from __future__ import annotations

import pytest

from core.messages import OperatorAlert
from mission.cost_map_alarm import CostMapAvoidanceAlarm


def test_two_failures_no_alert():
    a = CostMapAvoidanceAlarm(max_failures=3)
    a.record(success=False, reason="lethal cell ahead")
    a.record(success=False, reason="lethal cell ahead")
    assert a.poll_alert() is None
    assert a.consecutive_failures == 2


def test_three_failures_emits_one_alert():
    a = CostMapAvoidanceAlarm(max_failures=3)
    for _ in range(3):
        a.record(success=False, reason="reroute infeasible")
    alert = a.poll_alert()
    assert isinstance(alert, OperatorAlert)
    assert alert.code == "cost_map_avoidance_failed"
    assert alert.severity == "warning"
    assert alert.detail["consecutive_failures"] == 3
    assert alert.detail["last_reason"] == "reroute infeasible"
    assert a.total_alerts == 1


def test_alert_latches_until_success_resets():
    a = CostMapAvoidanceAlarm(max_failures=3)
    for _ in range(3):
        a.record(success=False, reason="x")
    assert a.poll_alert() is not None
    # Additional failures without a reset must NOT re-fire.
    a.record(success=False)
    a.record(success=False)
    assert a.poll_alert() is None
    assert a.total_alerts == 1


def test_success_resets_streak_then_refires():
    a = CostMapAvoidanceAlarm(max_failures=3)
    a.record(success=False)
    a.record(success=False)
    a.record(success=True)             # streak reset
    assert a.consecutive_failures == 0
    # New streak of 3 fires again.
    for _ in range(3):
        a.record(success=False, reason="new failure")
    assert a.poll_alert() is not None
    assert a.total_alerts == 1


def test_poll_alert_clears():
    a = CostMapAvoidanceAlarm(max_failures=2)
    a.record(False)
    a.record(False)
    first = a.poll_alert()
    second = a.poll_alert()
    assert first is not None
    assert second is None      # poll consumes


def test_max_failures_must_be_positive():
    with pytest.raises(ValueError):
        CostMapAvoidanceAlarm(max_failures=0)
