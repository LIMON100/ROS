"""End-to-end audit bus tests.

Validates the producer → bus → writer → hash-chained file pipeline that
main._audit_writer assembles from core.audit_log.publish_audit() and
core.ipc.audit_event.

Before this wiring, AuditLogger was correctness-tested in isolation but
nothing in production code called .log() — audit findings C1 / H1.
"""
from __future__ import annotations

import json
import tempfile
import threading
import time
from pathlib import Path

from core.audit_log import AuditLogger, publish_audit
from core.ipc import consume, make_topic_queues


def _spawn_writer(queues, logger, stop):
    """Start the same drain loop main._audit_writer uses, in a thread."""
    def _loop():
        while not stop.is_set():
            ev = consume(queues.audit_event, timeout=0.05)
            if ev is None:
                continue
            logger.log(
                category=ev.get("category", "unknown"),
                event=ev.get("event", "unknown"),
                actor=ev.get("actor"),
                params=ev.get("params") or {},
                fsm_phase=ev.get("fsm_phase"),
                mission_id=ev.get("mission_id"),
            )
    t = threading.Thread(target=_loop, name="AuditWriterTest", daemon=True)
    t.start()
    return t


def test_publish_audit_flows_through_bus_to_chained_log():
    queues = make_topic_queues(maxsize=20)
    with tempfile.TemporaryDirectory() as td:
        logger = AuditLogger(log_dir=td, robot_id="robot-test")
        stop = threading.Event()
        t = _spawn_writer(queues, logger, stop)

        publish_audit(queues, category="permission",
                      event="pin_auth_success", actor="ble:127.0.0.1")
        publish_audit(queues, category="mission", event="start",
                      mission_id="m-001",
                      params={"route": "patrol_north"})
        publish_audit(queues, category="permission", event="pin_auth_fail",
                      actor="ble:127.0.0.1",
                      params={"prev_attempts": 0})

        # Allow the drain loop to land all three writes
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            files = sorted(Path(td).glob("*.jsonl"))
            if files:
                lines = files[0].read_text().splitlines()
                if len(lines) >= 3:
                    break
            time.sleep(0.05)

        stop.set()
        t.join(timeout=1.0)

        files = sorted(Path(td).glob("*.jsonl"))
        assert files, "audit writer never created a file"
        lines = files[0].read_text().splitlines()
        assert len(lines) == 3, f"expected 3 audit lines, got {len(lines)}"

        entries = [json.loads(line) for line in lines]
        assert entries[0]["event"] == "pin_auth_success"
        assert entries[1]["category"] == "mission"
        assert entries[1]["mission_id"] == "m-001"
        assert entries[2]["params"]["prev_attempts"] == 0

        valid, bad_file, line_no = logger.verify_chain_all()
        assert valid, f"chain corrupted at {bad_file}:{line_no}"


def test_publish_audit_does_not_block_on_bus_overflow():
    """The bus uses publish() with drop-oldest semantics — flooding
    publishers must never block a producer thread."""
    queues = make_topic_queues(maxsize=20)
    # Don't start a writer — let the queue fill up + spill.
    for i in range(200):
        publish_audit(queues, category="comm",
                      event=f"floodfill_{i}",
                      params={"i": i})
    # If we got here without hanging, the producer side is non-blocking.
    # Sanity: queue depth shouldn't have grown past the configured cap
    # (overrides to 64 for audit_event in core.ipc.TOPIC_MAXSIZE).
    drained = 0
    while consume(queues.audit_event, timeout=0.01) is not None:
        drained += 1
        if drained > 256:        # guard against runaway
            break
    assert drained <= 64, drained
