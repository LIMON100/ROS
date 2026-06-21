# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 9-10 — Operational mode controller tests.

Coverage:
  M1  Default mode = RECON
  M2  Mode preset values match SDD Rev.A.6 §7.1
  M3  Successful mode change updates current
  M6  Unknown mode rejected
  M8  DEV_TEST max speed forced to 1.0 m/s
  M9  RECON mode default 1.3 m/s
  M10 Mode setter is idempotent

DCN-2026-023 v2 (2026-05-23): M4 / M5 / M7 PIN-auth tests removed —
the only production caller of set_pin_authenticated was the BLE
0xFF05 GATT challenge which DCN-2026-008 deleted. The mechanism
itself is gone in operational_modes.py.
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


# M3b (DCN-2026-024 — Option C):
# DEV_TEST requires a shared-secret token. Without one, request_mode
# fail-closes. With the correct token, succeeds.
def test_dev_test_requires_auth_token():
    # Fail-closed when no secret configured.
    c_locked = OperationalModeController(dev_test_secret_path="")
    ok, msg = c_locked.request_mode(OperationalMode.DEV_TEST)
    assert ok is False
    assert "none configured" in msg
    assert c_locked.current == OperationalMode.RECON

    # Token required + must match.
    c_ok = OperationalModeController(dev_test_secret="hunter2",
                                     dev_test_secret_path="")
    ok, msg = c_ok.request_mode(OperationalMode.DEV_TEST,
                                auth_token="hunter2")
    assert ok is True
    assert c_ok.current == OperationalMode.DEV_TEST

    # Wrong token → reject + state unchanged.
    c_wrong = OperationalModeController(dev_test_secret="hunter2",
                                        dev_test_secret_path="")
    ok, msg = c_wrong.request_mode(OperationalMode.DEV_TEST,
                                   auth_token="wrong")
    assert ok is False
    assert "mismatch" in msg
    assert c_wrong.current == OperationalMode.RECON


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


# M8
def test_dev_test_max_speed_1_0():
    # DCN-024: pass token so DEV_TEST is allowed; then check speed cap.
    c = OperationalModeController(dev_test_secret="t",
                                  dev_test_secret_path="")
    ok, _ = c.request_mode(OperationalMode.DEV_TEST, auth_token="t")
    assert ok
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


# ─── DCN-2026-024 — secret resolution priority + diagnostics ───────────

# M11 — secret resolution: explicit ctor arg wins over env + file.
def test_secret_resolution_explicit_wins(monkeypatch, tmp_path):
    secret_file = tmp_path / "dev_test_secret"
    secret_file.write_text("from-file\n")
    monkeypatch.setenv("SAN_DEV_TEST_SECRET", "from-env")
    c = OperationalModeController(dev_test_secret="from-arg",
                                  dev_test_secret_path=str(secret_file))
    ok, _ = c.request_mode(OperationalMode.DEV_TEST,
                           auth_token="from-arg")
    assert ok
    assert c.dev_test_secret_loaded()


# M12 — env var fallback when no explicit arg.
def test_secret_resolution_env_fallback(monkeypatch, tmp_path):
    monkeypatch.setenv("SAN_DEV_TEST_SECRET", "from-env")
    c = OperationalModeController(dev_test_secret=None,
                                  dev_test_secret_path="")
    ok, _ = c.request_mode(OperationalMode.DEV_TEST,
                           auth_token="from-env")
    assert ok


# M13 — file fallback when no explicit + env.
def test_secret_resolution_file_fallback(monkeypatch, tmp_path):
    monkeypatch.delenv("SAN_DEV_TEST_SECRET", raising=False)
    secret_file = tmp_path / "dev_test_secret"
    secret_file.write_text("from-file\n")
    c = OperationalModeController(dev_test_secret=None,
                                  dev_test_secret_path=str(secret_file))
    ok, _ = c.request_mode(OperationalMode.DEV_TEST,
                           auth_token="from-file")
    assert ok


# M14 — fail-closed: no source yields → DEV_TEST locked out.
def test_secret_resolution_fail_closed(monkeypatch):
    monkeypatch.delenv("SAN_DEV_TEST_SECRET", raising=False)
    c = OperationalModeController(dev_test_secret=None,
                                  dev_test_secret_path="/nonexistent/path")
    assert not c.dev_test_secret_loaded()
    ok, msg = c.request_mode(OperationalMode.DEV_TEST,
                             auth_token="anything")
    assert not ok
    assert "none configured" in msg


# M15 — diagnostics: dev_test_secret_loaded() never leaks the secret.
def test_dev_test_secret_loaded_no_leak():
    c = OperationalModeController(dev_test_secret="hunter2",
                                  dev_test_secret_path="")
    assert c.dev_test_secret_loaded() is True
    # secret value never reachable through public surface.
    assert not hasattr(c, "secret")
    assert not hasattr(c, "auth_token")
