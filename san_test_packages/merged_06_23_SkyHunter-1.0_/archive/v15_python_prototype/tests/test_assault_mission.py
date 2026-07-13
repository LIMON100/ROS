"""Tests for M4 assault mode (P2-8)."""
from __future__ import annotations

from pathlib import Path

import pytest
import yaml

from mission.assault_mission import AssaultMissionValidator

VALID_CONFIG = {
    "type": "assault",
    "formation": {"type": "V_SHAPE", "d_m": 15.0, "theta_deg": 60.0},
    "leader_max_speed_mps": 1.3,
}


def test_valid_config_passes():
    ok, _msg = AssaultMissionValidator.validate(VALID_CONFIG)
    assert ok is True


def test_d_m_must_be_15():
    cfg = dict(VALID_CONFIG)
    cfg["formation"] = dict(cfg["formation"])
    cfg["formation"]["d_m"] = 10.0
    ok, msg = AssaultMissionValidator.validate(cfg)
    assert ok is False
    assert "15" in msg


def test_theta_must_be_60():
    cfg = dict(VALID_CONFIG)
    cfg["formation"] = dict(cfg["formation"])
    cfg["formation"]["theta_deg"] = 90.0
    ok, msg = AssaultMissionValidator.validate(cfg)
    assert ok is False
    assert "60" in msg


def test_speed_limit_enforced():
    cfg = dict(VALID_CONFIG)
    cfg["leader_max_speed_mps"] = 2.0
    ok, _msg = AssaultMissionValidator.validate(cfg)
    assert ok is False


def test_yaml_dict_to_assault_config():
    config = AssaultMissionValidator.from_yaml_dict(VALID_CONFIG)
    assert config.d_m == 15.0
    assert config.theta_deg == 60.0


def test_yaml_file_loadable():
    """tactical_missions.yaml stores missions as a list of dicts (legacy
    format); find M4_assault by name."""
    yaml_path = Path("config/tactical_missions.yaml")
    if not yaml_path.exists():
        pytest.skip("tactical_missions.yaml not found")
    with yaml_path.open(encoding="utf-8") as f:
        data = yaml.safe_load(f)
    missions = data.get("missions", [])
    m4 = next((m for m in missions if m.get("name") == "M4_assault"), None)
    assert m4 is not None, "M4_assault mission must be defined"
    assert m4.get("type") == "assault"
    ok, _msg = AssaultMissionValidator.validate(m4)
    assert ok is True
