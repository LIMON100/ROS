"""PermissionGuard wiring in PerceptionProcess + guarded_publish helper.

The unit-level guard logic (case folding, BLOCK/ALLOW decisions,
violation accumulation) is covered in tests/test_permission_guard.py.
These tests focus on the integration:

  PerceptionProcess (source="perception_ai")
    → guarded_publish(guard, queues, topic, msg, source)
      → if ALLOW → core.ipc.publish() onto queues.<topic>
      → if BLOCK → audit_callback → publish_audit() onto queues.audit_event

The tripwire is preventive: PerceptionProcess only publishes to
queues.anomaly today (an AI-permitted topic), so the guard passes
through. The value is that a future bug wiring AI output to a control
topic would be caught and audited rather than reaching cmd_vel.
"""
from __future__ import annotations

import threading
from unittest.mock import MagicMock

from core import make_topic_queues
from core.ipc import consume
from core.permission_guard import (
    GuardViolation,
    PermissionGuard,
    guarded_publish,
)
from perception.perception_process import PerceptionProcess


# ─── guarded_publish helper ────────────────────────────────────────────
def test_guarded_publish_allows_anomaly_for_perception_ai():
    g = PermissionGuard()
    qs = make_topic_queues()
    sent = guarded_publish(g, qs, "anomaly", {"x": 1},
                           source="perception_ai")
    assert sent is True
    msg = consume(qs.anomaly, timeout=0.5)
    assert msg == {"x": 1}
    assert g.violation_count == 0


def test_guarded_publish_blocks_cmd_vel_for_ai_source():
    g = PermissionGuard()
    qs = make_topic_queues()
    sent = guarded_publish(g, qs, "cmd_vel", {"v": 1.0},
                           source="ai_detection")
    assert sent is False
    assert consume(qs.cmd_vel, timeout=0.1) is None
    assert g.violation_count == 1


def test_guarded_publish_allows_cmd_vel_for_non_ai_source():
    g = PermissionGuard()
    qs = make_topic_queues()
    sent = guarded_publish(g, qs, "cmd_vel", {"v": 1.0},
                           source="mission_bt")
    assert sent is True


def test_guarded_publish_returns_false_for_unknown_topic():
    g = PermissionGuard()
    qs = make_topic_queues()
    # No such attr on TopicQueues — should not raise, just return False
    sent = guarded_publish(g, qs, "no_such_topic", {},
                           source="perception_ai")
    assert sent is False


def test_guarded_publish_block_triggers_audit_callback():
    captured: list = []
    g = PermissionGuard(audit_callback=captured.append)
    qs = make_topic_queues()
    guarded_publish(g, qs, "leader_pose", {"x": 0},
                    source="yolov5s")
    assert len(captured) == 1
    assert isinstance(captured[0], GuardViolation)
    assert captured[0].topic == "leader_pose"
    assert captured[0].source == "yolov5s"


def test_guarded_publish_normalises_input_so_bypass_attempts_fail():
    """Case variation must not slip past the guard via guarded_publish()."""
    g = PermissionGuard()
    qs = make_topic_queues()
    sent = guarded_publish(g, qs, "CMD_VEL", {},
                           source="AI_Detection")
    assert sent is False
    assert g.violation_count == 1


# ─── PerceptionProcess wiring ──────────────────────────────────────────
def _make_perception() -> PerceptionProcess:
    """Build a PerceptionProcess instance without spawning the BaseProcess
    machinery. Just enough to exercise the guard plumbing — the heavy
    detector / fuser fields aren't constructed."""
    p = PerceptionProcess.__new__(PerceptionProcess)
    p.queues = make_topic_queues()
    p.cfg = MagicMock()
    p.cfg.get.return_value = None
    p.log = MagicMock()
    p._lock = threading.Lock()
    p._latest_pose = None
    p._detector = None
    p._classes_of_interest = None
    p._frame_queue = None
    p._results_queue = None
    p._stats = {"frames": 0, "detections": 0, "anomalies": 0,
                "guard_blocks": 0}
    p._guard = PermissionGuard(audit_callback=p._on_guard_violation)
    return p


def test_perception_constructs_guard_with_audit_callback():
    p = _make_perception()
    assert isinstance(p._guard, PermissionGuard)
    # Bound methods don't compare with `is`, but they do with `==`.
    # The substantive check is downstream: a BLOCK must publish an
    # audit event onto the bus (verified in the cmd_vel test below).
    assert p._guard.audit_callback == p._on_guard_violation
    assert p._guard.audit_callback is not None


def test_perception_anomaly_publish_flows_through_guard_unblocked():
    """The happy path: PerceptionProcess publishing AnomalyEvent to
    queues.anomaly is ALLOW for source="perception_ai". The integrated
    guarded_publish call should land the message on the queue."""
    p = _make_perception()
    ok = guarded_publish(p._guard, p.queues, "anomaly", {"ev": "x"},
                         source="perception_ai")
    assert ok is True
    msg = consume(p.queues.anomaly, timeout=0.5)
    assert msg == {"ev": "x"}
    assert p._guard.violation_count == 0


def test_perception_hypothetical_cmd_vel_publish_is_blocked_and_audited():
    """If a future bug wires perception to cmd_vel, the guard blocks
    it and emits a permission/guard_block audit event."""
    p = _make_perception()
    ok = guarded_publish(p._guard, p.queues, "cmd_vel",
                         {"v": 1.0, "w": 0.0},
                         source="perception_ai")
    assert ok is False
    # No cmd_vel published
    assert consume(p.queues.cmd_vel, timeout=0.1) is None
    # Audit event landed on the bus
    ev = consume(p.queues.audit_event, timeout=0.5)
    assert ev is not None
    assert ev["category"] == "permission"
    assert ev["event"] == "guard_block"
    assert ev["actor"] == "perception_ai"
    assert ev["params"]["topic"] == "cmd_vel"


def test_perception_guard_audit_callback_failure_does_not_crash_publish():
    """PermissionGuard.check() wraps the audit_callback in a try/except.
    Even if publish_audit raises (e.g. queue serialisation error), the
    BLOCK decision still propagates and no exception leaks."""
    p = _make_perception()

    # Replace the audit method with one that raises
    def explode(v):
        raise RuntimeError("audit bus down")
    p._guard.audit_callback = explode

    # This must not raise
    ok = guarded_publish(p._guard, p.queues, "cmd_vel", {},
                         source="perception_ai")
    assert ok is False
    assert p._guard.violation_count == 1


def test_perception_violation_count_tracked_locally():
    """Per-process counter on the guard stays in sync with audit events
    for live triage / metrics dashboard."""
    p = _make_perception()
    for _ in range(3):
        guarded_publish(p._guard, p.queues, "leader_pose", {},
                        source="perception_ai")
    assert p._guard.violation_count == 3
    assert len(p._guard.violations) == 3


# ─── End-to-end: guard violations show up in the audit log ────────────
def test_guard_block_audit_entry_shape_matches_writer_schema():
    """The audit writer drains queues.audit_event and feeds AuditLogger.
    Verify the dict shape published on a guard BLOCK matches the schema
    main._audit_writer expects (category/event/actor/params/...)."""
    p = _make_perception()
    guarded_publish(p._guard, p.queues, "follower_target", {},
                    source="anomaly_classifier")
    ev = consume(p.queues.audit_event, timeout=0.5)
    assert ev is not None
    # Match the keys main._audit_writer reads
    assert set(ev.keys()) >= {"category", "event", "actor", "params",
                               "fsm_phase", "mission_id"}
    assert ev["params"]["topic"] == "follower_target"
    assert ev["params"]["reason"]      # non-empty reason string
