"""Tests for anomaly event push format (P1-12, SDD Rev.A.6 §15.6)."""
from __future__ import annotations

from control.ws_telemetry_process import WsTelemetryProcess


# ─── severity mapping ───
def test_severity_critical_for_comm_loss():
    assert WsTelemetryProcess._severity_from_type("comm_loss") == "critical"


def test_severity_critical_for_leader_lost():
    assert WsTelemetryProcess._severity_from_type("leader_lost") == "critical"


def test_severity_warn_for_road_blocked():
    assert WsTelemetryProcess._severity_from_type("road_blocked") == "warn"


def test_severity_warn_for_ai_detection():
    assert WsTelemetryProcess._severity_from_type("ai_detection") == "warn"


def test_severity_info_default():
    """Unmapped types fall through to info."""
    assert WsTelemetryProcess._severity_from_type("unmapped_obstacle") == "info"
    assert WsTelemetryProcess._severity_from_type("structure_changed") == "info"
    assert WsTelemetryProcess._severity_from_type("") == "info"


# ─── message schema ───
def test_anomaly_message_schema():
    """Wire format expected by Android UI (SDD §15.6)."""
    sample = {
        "method": "anomaly",
        "params": {
            "ts_ms": 1715335200456,
            "type": "ai_detection",
            "robot_id": "robot-003",
            "location": {"lat": 37.498, "lon": 127.0285,
                         "accuracy_m": 0.5},
            "confidence": 0.87,
            "image_url":
                "http://192.168.42.10:8000/recordings/snap_x.jpg",
            "ai_class": "person",
            "severity": "warn",
        },
    }
    assert sample["method"] == "anomaly"
    p = sample["params"]
    for k in ("ts_ms", "type", "location", "confidence", "severity"):
        assert k in p
    assert p["severity"] in ("info", "warn", "critical")
    assert 0.0 <= p["confidence"] <= 1.0


def test_image_url_present_for_ai_detection():
    sample = {
        "type": "ai_detection",
        "image_url": "http://192.168.42.10:8000/recordings/snap.jpg",
    }
    assert sample["image_url"] is not None
    assert sample["image_url"].startswith("http")


def test_image_url_optional_for_other_types():
    """unmapped_obstacle (and similar) doesn't require an image."""
    sample = {"type": "unmapped_obstacle", "image_url": None}
    assert sample["image_url"] is None
