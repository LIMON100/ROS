"""Tests for AI permission invariants (P2-14, SDD §8)."""
from __future__ import annotations

import pytest

from core.permission_guard import (
    GuardDecision,
    GuardViolation,
    PermissionGuard,
)


def test_ai_blocked_from_cmd_vel():
    g = PermissionGuard()
    assert g.check(topic="cmd_vel", source="ai_detection") \
        == GuardDecision.BLOCK
    assert g.violation_count == 1


def test_ai_blocked_from_leader_pose():
    g = PermissionGuard()
    assert g.check(topic="leader_pose", source="yolov5s") \
        == GuardDecision.BLOCK


def test_ai_blocked_from_formation_change():
    g = PermissionGuard()
    assert g.check(topic="formation_change", source="perception_ai") \
        == GuardDecision.BLOCK


def test_ai_allowed_to_anomaly_events():
    g = PermissionGuard()
    assert g.check(topic="anomaly_events", source="ai_detection") \
        == GuardDecision.ALLOW
    assert g.violation_count == 0


def test_ai_allowed_to_alert():
    g = PermissionGuard()
    assert g.check(topic="alert", source="anomaly_classifier") \
        == GuardDecision.ALLOW


def test_mission_bt_allowed_to_cmd_vel():
    g = PermissionGuard()
    assert g.check(topic="cmd_vel", source="mission_bt") \
        == GuardDecision.ALLOW


def test_orchestrator_allowed_to_cmd_vel():
    g = PermissionGuard()
    assert g.check(topic="cmd_vel", source="orchestrator") \
        == GuardDecision.ALLOW


def test_audit_callback_invoked():
    fired: list = []
    g = PermissionGuard(audit_callback=fired.append)
    g.check(topic="cmd_vel", source="ai_detection")
    assert len(fired) == 1
    assert isinstance(fired[0], GuardViolation)
    assert fired[0].topic == "cmd_vel"
    assert fired[0].source == "ai_detection"


def test_audit_callback_only_on_violation():
    fired: list = []
    g = PermissionGuard(audit_callback=fired.append)
    g.check(topic="anomaly_events", source="ai_detection")
    assert fired == []


def test_violations_list_accumulates():
    g = PermissionGuard()
    g.check(topic="cmd_vel", source="ai_detection")
    g.check(topic="leader_pose", source="yolov5s")
    g.check(topic="anomaly_events", source="ai_detection")  # ALLOW
    assert len(g.violations) == 2


def test_reset_violations():
    g = PermissionGuard()
    g.check(topic="cmd_vel", source="ai_detection")
    assert g.violation_count == 1
    g.reset_violations()
    assert g.violation_count == 0
    assert g.violations == []


def test_audit_callback_failure_does_not_crash():
    """Guard must not propagate audit callback exceptions."""
    def bad_callback(v):
        raise RuntimeError("audit log unreachable")

    g = PermissionGuard(audit_callback=bad_callback)
    decision = g.check(topic="cmd_vel", source="ai_detection")
    assert decision == GuardDecision.BLOCK
    assert g.violation_count == 1


# ─── Bypass-resistance (H5 fix) ───
@pytest.mark.parametrize("source", [
    "AI_Detection",           # case
    "AI_DETECTION",           # upper
    " ai_detection",          # leading whitespace
    "ai_detection ",          # trailing whitespace
    "ai_detection\x00",       # embedded NUL
    "ai_​detection",     # zero-width space between '_' and 'd'
    "﻿ai_detection",     # BOM prefix
])
def test_normalized_source_still_blocks(source):
    g = PermissionGuard()
    assert g.check(topic="cmd_vel", source=source) == GuardDecision.BLOCK


@pytest.mark.parametrize("topic", [
    "CMD_VEL",
    " cmd_vel",
    "cmd_vel\x00",
    "Cmd_Vel",
])
def test_normalized_topic_still_blocks(topic):
    g = PermissionGuard()
    assert g.check(topic=topic, source="ai_detection") == GuardDecision.BLOCK


def test_non_string_inputs_dont_crash():
    """Defensive: None or int inputs from a misbehaving caller should
    not raise from inside the guard."""
    g = PermissionGuard()
    assert g.check(topic="cmd_vel", source=None) == GuardDecision.ALLOW  # type: ignore[arg-type]
    assert g.check(topic=None, source="ai_detection") == GuardDecision.ALLOW  # type: ignore[arg-type]
    assert g.check(topic=42, source=42) == GuardDecision.ALLOW  # type: ignore[arg-type]
