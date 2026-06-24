"""Tests for 5 operational mode presets (P1-14, SDD Rev.A.6 §7.1)."""
from __future__ import annotations

from mission.operational_modes import (
    PRESETS,
    OperationalMode,
    OperationalModeController,
)


# ─── preset table contents ───
def test_5_presets_exist():
    assert len(PRESETS) == 5
    for m in (OperationalMode.DEV_TEST, OperationalMode.NARROW,
              OperationalMode.RECON, OperationalMode.WIDE,
              OperationalMode.ASSAULT):
        assert m in PRESETS


def test_dev_test_d_3m():
    """User decision #7: Test mode = 3 m."""
    assert PRESETS[OperationalMode.DEV_TEST].d_m == 3.0


def test_recon_d_5m():
    """User decision #7: 이동 mode = 5 m."""
    assert PRESETS[OperationalMode.RECON].d_m == 5.0


def test_assault_d_15m():
    """User decision #7: 돌격 mode = 15 m."""
    assert PRESETS[OperationalMode.ASSAULT].d_m == 15.0


def test_dev_test_speed_forced_1_0_mps():
    """SDD §7.1: dev mode forces 1.0 m/s."""
    assert PRESETS[OperationalMode.DEV_TEST].leader_max_speed_mps == 1.0


def test_other_modes_speed_1_3_mps():
    for mode in (OperationalMode.NARROW, OperationalMode.RECON,
                 OperationalMode.WIDE, OperationalMode.ASSAULT):
        assert PRESETS[mode].leader_max_speed_mps == 1.3


def test_dev_test_requires_pin():
    assert PRESETS[OperationalMode.DEV_TEST].requires_pin is True


def test_other_modes_no_pin():
    for mode in (OperationalMode.NARROW, OperationalMode.RECON,
                 OperationalMode.WIDE, OperationalMode.ASSAULT):
        assert PRESETS[mode].requires_pin is False


# ─── controller behavior ───
def test_default_mode_is_recon():
    ctl = OperationalModeController()
    assert ctl.current == OperationalMode.RECON


def test_request_dev_mode_blocked_without_pin():
    ctl = OperationalModeController()
    ok, msg = ctl.request_mode(OperationalMode.DEV_TEST)
    assert ok is False
    assert "PIN" in msg
    # Mode stays at the default
    assert ctl.current == OperationalMode.RECON


def test_request_dev_mode_allowed_with_pin():
    ctl = OperationalModeController()
    ctl.set_pin_authenticated(True)
    ok, msg = ctl.request_mode(OperationalMode.DEV_TEST)
    assert ok is True
    assert ctl.current == OperationalMode.DEV_TEST


def test_get_max_speed_in_dev_mode():
    ctl = OperationalModeController()
    ctl.set_pin_authenticated(True)
    ctl.request_mode(OperationalMode.DEV_TEST)
    assert ctl.get_max_speed() == 1.0


def test_get_max_speed_in_assault():
    ctl = OperationalModeController()
    ctl.request_mode(OperationalMode.ASSAULT)
    assert ctl.get_max_speed() == 1.3


def test_request_mode_returns_helpful_message():
    ctl = OperationalModeController()
    ok, msg = ctl.request_mode(OperationalMode.RECON)
    assert ok is True
    assert "recon" in msg
