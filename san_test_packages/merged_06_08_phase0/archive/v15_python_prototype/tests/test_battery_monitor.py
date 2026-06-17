"""Tests for battery RTH (P1-19, SDD Rev.A.6 §8)."""
from __future__ import annotations

from safety.battery_monitor import BatteryAction, BatteryMonitor


# ─── Threshold mapping ───
def test_full_battery_no_action():
    bm = BatteryMonitor()
    assert bm.update(85.0) == BatteryAction.NONE


def test_warn_at_25_pct():
    bm = BatteryMonitor()
    assert bm.update(25.0) == BatteryAction.WARN


def test_rth_at_20_pct():
    bm = BatteryMonitor()
    assert bm.update(20.0) == BatteryAction.RTH


def test_rth_at_19_pct():
    bm = BatteryMonitor()
    assert bm.update(19.0) == BatteryAction.RTH


def test_emergency_at_10_pct():
    bm = BatteryMonitor()
    assert bm.update(10.0) == BatteryAction.EMERGENCY


def test_emergency_at_8_pct():
    bm = BatteryMonitor()
    assert bm.update(8.0) == BatteryAction.EMERGENCY


# ─── Override / callback / hysteresis ───
def test_dev_override_skips_action():
    bm = BatteryMonitor(dev_override=True)
    assert bm.update(5.0) == BatteryAction.NONE


def test_callback_invoked_on_change():
    fired: list = []
    bm = BatteryMonitor(on_action=lambda a, s: fired.append((a, s)))
    bm.update(85.0)                    # NONE → no callback
    bm.update(19.0)                    # RTH → callback
    assert len(fired) == 1
    assert fired[0][0] == BatteryAction.RTH


def test_hysteresis_prevents_thrashing():
    """Once RTH fires, recovery requires SoC ≥ 22 %."""
    bm = BatteryMonitor()
    bm.update(19.0)
    assert bm.current_action == BatteryAction.RTH
    bm.update(20.5)                    # within hysteresis margin (≤ 22 %)
    assert bm.current_action == BatteryAction.RTH
    bm.update(23.0)                    # above margin → drops out of RTH
    assert bm.current_action != BatteryAction.RTH


def test_progression_through_levels():
    bm = BatteryMonitor()
    assert bm.update(85.0) == BatteryAction.NONE
    assert bm.update(28.0) == BatteryAction.WARN
    assert bm.update(20.0) == BatteryAction.RTH
    assert bm.update(9.0) == BatteryAction.EMERGENCY
