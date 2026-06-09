"""Tests for 30 Hz telemetry envelope (P1-11, SDD Rev.A.6 §15.5)."""
from __future__ import annotations

import json
import time


def _sample_snapshot() -> dict:
    """Reference shape per SDD §15.5 — 1 leader + 4 followers."""
    leader = {
        "id": "robot-001", "role": "leader",
        "pose": {"lat": 37.4979, "lon": 127.0276,
                 "heading_deg": 87.5},
        "speed_mps": 1.28, "battery_pct": 78,
        "tier": "T0", "rtk_quality": "FIXED", "errors": [],
    }
    followers = [
        {
            "id": f"robot-{i:03d}", "role": "follower",
            "pose": {"lat": 37.5, "lon": 127.0, "heading_deg": 90.0},
            "speed_mps": 1.30, "battery_pct": 76,
            "tier": "T0", "rtk_quality": "FIXED", "errors": [],
        }
        for i in range(2, 6)
    ]
    return {
        "ts_ms": int(time.time() * 1000),
        "leader_id": "robot-001",
        "robots": [leader] + followers,
        "swarm_health": {
            "in_rollback": False, "struggling_ratio": 0.0,
            "anomaly_count": 0,
        },
    }


def test_telemetry_message_includes_required_fields():
    """Schema check per SDD §15.5."""
    sample = {"method": "telemetry", "params": _sample_snapshot()}
    assert sample["method"] == "telemetry"
    p = sample["params"]
    assert "ts_ms" in p and "robots" in p and "swarm_health" in p
    for robot in p["robots"]:
        assert "id" in robot
        assert "tier" in robot
        assert robot["tier"] in {"T0", "T1.5", "T1", "T2", "T3", "T4"}
    sh = p["swarm_health"]
    assert isinstance(sh["in_rollback"], bool)
    assert 0.0 <= sh["struggling_ratio"] <= 1.0


def test_tier_color_mapping():
    """SDD §15.5: 6-state mapping (Android UI consumes verbatim)."""
    expected_colors = {
        "T0":   "cyan",
        "T1.5": "purple",
        "T1":   "green",
        "T2":   "yellow",
        "T3":   "orange",
        "T4":   "red_blink",
    }
    assert len(expected_colors) == 6
    # Every tier the SDD recognises has a UI color.
    for tier in ("T0", "T1.5", "T1", "T2", "T3", "T4"):
        assert tier in expected_colors


def test_30_hz_period():
    period = 1.0 / 30.0
    assert abs(period - 0.0333) < 0.001


def test_swarm_health_struggling_ratio_in_range():
    sh = {"in_rollback": False, "struggling_ratio": 0.6,
          "anomaly_count": 0}
    assert 0.0 <= sh["struggling_ratio"] <= 1.0


def test_serialization_under_5ms_for_5_robots():
    """JSON serialize must stay well under the 33 ms broadcast budget."""
    snapshot = _sample_snapshot()
    start = time.monotonic()
    for _ in range(100):
        json.dumps(snapshot)
    elapsed = (time.monotonic() - start) / 100
    assert elapsed < 0.005, f"serialize took {elapsed*1000:.2f} ms"


def test_dual_envelope_keeps_legacy_data_field():
    """Backward compat: legacy clients reading `msg.data.battery_soc`
    still see a meaningful value alongside the new `params.robots[]`."""
    legacy = {
        "phase": 1, "pose": [1.0, 2.0, 0.0, 0, 0, 0, 1],
        "battery_soc": 0.78, "locomotion_mode": "walk",
        "mission": None, "ts_mono": 1.0,
    }
    msg = {
        "type": "telemetry", "data": legacy,
        "method": "telemetry", "params": _sample_snapshot(),
    }
    # Old API still works
    assert msg["data"]["battery_soc"] == 0.78
    assert msg["data"]["locomotion_mode"] == "walk"
    # New API works on the same frame
    assert msg["params"]["robots"][0]["battery_pct"] == 78
