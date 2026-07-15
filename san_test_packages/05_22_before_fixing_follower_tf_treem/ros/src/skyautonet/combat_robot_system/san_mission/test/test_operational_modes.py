"""SAN v1.5 Phase 2-E Turn 9-10 — Operational mode controller tests.

Coverage:
  M1  Default mode = RECON
  M2  Mode preset values match SDD Rev.A.6 §7.1
  M3  Successful mode change updates current
  M4  DEV_TEST without PIN auth fails
  M5  DEV_TEST with PIN auth succeeds
  M6  Unknown mode rejected
  M7  PIN authentication is idempotent
  M8  DEV_TEST max speed forced to 1.0 m/s
  M9  RECON mode default 1.3 m/s
  M10 Mode setter is idempotent
"""

from san_mission.operational_modes import (
    PRESETS,
    OperationalMode,
    OperationalModeController,
)


# M1
def test_default_mode_is_recon():
    c = OperationalModeController()
    assert c.current == OperationalMode.RECON


# M2
def test_preset_values_match_sdd():
    dev   = PRESETS[OperationalMode.DEV_TEST]
    narrow = PRESETS[OperationalMode.NARROW]
    recon = PRESETS[OperationalMode.RECON]
    wide  = PRESETS[OperationalMode.WIDE]
    assault = PRESETS[OperationalMode.ASSAULT]

    assert dev.d_m == 3.0
    assert dev.leader_max_speed_mps == 1.0
    assert dev.requires_pin is True

    assert narrow.d_m == 3.0
    assert narrow.theta_deg == 40.0

    assert recon.d_m == 5.0
    assert recon.theta_deg == 90.0
    assert recon.leader_max_speed_mps == 1.3

    assert wide.d_m == 7.0
    assert assault.d_m == 15.0


# M3
def test_request_mode_updates_current():
    c = OperationalModeController()
    ok, _ = c.request_mode(OperationalMode.WIDE)
    assert ok
    assert c.current == OperationalMode.WIDE


# M4
def test_dev_test_without_pin_fails():
    c = OperationalModeController()
    assert c.is_pin_authenticated() is False
    ok, msg = c.request_mode(OperationalMode.DEV_TEST)
    assert ok is False
    assert "PIN" in msg
    assert c.current == OperationalMode.RECON


# M5
def test_dev_test_with_pin_succeeds():
    c = OperationalModeController()
    c.set_pin_authenticated(True)
    ok, _ = c.request_mode(OperationalMode.DEV_TEST)
    assert ok is True
    assert c.current == OperationalMode.DEV_TEST


# M6
def test_unknown_mode_rejected():
    c = OperationalModeController()
    # Bypass enum validation to feed in a synthetic value:
    fake_mode = "garbage"
    try:
        ok, _ = c.request_mode(fake_mode)
        # If no exception, request_mode returned False
        assert ok is False
    except (KeyError, ValueError, AttributeError, TypeError):
        # Acceptable — enum machinery rejects it before us
        pass


# M7
def test_pin_authentication_idempotent():
    c = OperationalModeController()
    c.set_pin_authenticated(True)
    c.set_pin_authenticated(True)
    assert c.is_pin_authenticated() is True


# M8
def test_dev_test_max_speed_1_0():
    c = OperationalModeController()
    c.set_pin_authenticated(True)
    c.request_mode(OperationalMode.DEV_TEST)
    assert c.get_max_speed() == 1.0


# M9
def test_recon_max_speed_1_3():
    c = OperationalModeController()
    assert c.get_max_speed() == 1.3


# M10
def test_mode_setter_idempotent():
    c = OperationalModeController()
    c.request_mode(OperationalMode.NARROW)
    c.request_mode(OperationalMode.NARROW)
    c.request_mode(OperationalMode.NARROW)
    assert c.current == OperationalMode.NARROW
