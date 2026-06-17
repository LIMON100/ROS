"""Tests for audit log hash chain (P1-16, SDD Rev.A.6 §9.7)."""
from __future__ import annotations

import json
import tempfile
import threading
from pathlib import Path

import pytest

from core.audit_log import GENESIS_HASH, AuditLogger


@pytest.fixture
def tmp_logger():
    with tempfile.TemporaryDirectory() as td:
        yield AuditLogger(log_dir=td, robot_id="robot-test")


# ─── Chain construction ───
def test_first_entry_uses_genesis_hash(tmp_logger):
    h = tmp_logger.log("mission", "start", params={"mission_id": "M1"})
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    assert len(files) == 1
    entry = json.loads(files[0].read_text().strip())
    assert entry["prev_hash"] == GENESIS_HASH
    assert entry["self_hash"] == h
    assert len(h) == 64


def test_second_entry_links_to_first(tmp_logger):
    h1 = tmp_logger.log("mission", "start")
    h2 = tmp_logger.log("mission", "complete")
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    lines = files[0].read_text().strip().splitlines()
    assert len(lines) == 2
    e1, e2 = json.loads(lines[0]), json.loads(lines[1])
    assert e1["self_hash"] == h1
    assert e2["prev_hash"] == h1
    assert e2["self_hash"] == h2


# ─── Verification ───
def test_verify_chain_valid(tmp_logger):
    for i in range(5):
        tmp_logger.log("safety", f"event_{i}", params={"value": i})
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    valid, first_bad = tmp_logger.verify_chain(files[0])
    assert valid is True
    assert first_bad == 0


def test_verify_chain_detects_tampering(tmp_logger):
    tmp_logger.log("safety", "event_0")
    tmp_logger.log("safety", "event_1")
    tmp_logger.log("safety", "event_2")
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    lines = files[0].read_text().strip().splitlines()
    e = json.loads(lines[1])
    e["params"] = {"tampered": True}            # change without re-hash
    lines[1] = json.dumps(e, sort_keys=True)
    files[0].write_text("\n".join(lines) + "\n")
    valid, first_bad = tmp_logger.verify_chain(files[0])
    assert valid is False
    assert first_bad == 2


# ─── Restart resumption ───
def test_resume_chain_after_restart(tmp_logger):
    h1 = tmp_logger.log("safety", "event_0")
    # Same dir, fresh logger instance
    logger2 = AuditLogger(log_dir=str(tmp_logger.log_dir),
                          robot_id="robot-test")
    h2 = logger2.log("safety", "event_1")
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    lines = files[0].read_text().strip().splitlines()
    e2 = json.loads(lines[-1])
    assert e2["prev_hash"] == h1
    assert e2["self_hash"] == h2
    # And the resumed chain is end-to-end valid.
    valid, _ = logger2.verify_chain(files[0])
    assert valid is True


# ─── Schema breadth ───
def test_categories_supported(tmp_logger):
    cats = ["mission", "fsm", "permission", "safety",
            "comm", "ai_decision", "election", "time_sync"]
    for c in cats:
        tmp_logger.log(c, f"test_{c}")
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    lines = files[0].read_text().strip().splitlines()
    assert len(lines) == 8


def test_entry_includes_required_fields(tmp_logger):
    tmp_logger.log("permission", "dev_mode_enabled",
                   actor="operator-12345",
                   params={"pin_attempts": 1},
                   fsm_phase="STREAMING",
                   mission_id="M_20260512")
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    e = json.loads(files[0].read_text().strip())
    for field in ("ts_iso", "robot_id", "category", "event",
                  "actor", "params", "fsm_phase", "mission_id",
                  "prev_hash", "self_hash"):
        assert field in e


# ─── Concurrency / robustness ───
def test_concurrent_logs_thread_safe(tmp_logger):
    def worker(n):
        for i in range(n):
            tmp_logger.log("safety", f"thread_evt_{i}")

    threads = [threading.Thread(target=worker, args=(20,))
               for _ in range(5)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    files = list(Path(tmp_logger.log_dir).glob("*.jsonl"))
    valid, _ = tmp_logger.verify_chain(files[0])
    assert valid is True


def test_disk_full_does_not_crash(tmp_logger):
    """A failing write must not propagate — mission keeps running.

    Originally this used log_dir="/nonexistent/cannot/create/here", which
    on Linux failed to mkdir (no root). On Windows, however,
    Path.mkdir(parents=True, exist_ok=True) interprets the leading "/"
    as the current drive root and SUCCEEDS, creating
    C:\\nonexistent\\cannot\\create\\here as a side-effect. That stale
    directory then defeated test_camera_fanout's "/nonexistent" device
    probe (os.path.exists became True, IMX678Adapter stayed out of stub
    mode, step() never published, 5 fanout tests failed for two
    sprints). Now we use a writable tmp dir and delete it post-init.
    """
    import shutil
    shutil.rmtree(tmp_logger.log_dir)
    tmp_logger.log("safety", "test")     # must not raise


# ─── Drop observability (audit critical fix) ───
def test_dropped_count_increments_on_write_failure(tmp_logger):
    """Force a write failure by deleting the log dir after init."""
    import shutil
    assert tmp_logger.dropped_count == 0
    shutil.rmtree(tmp_logger.log_dir)
    tmp_logger.log("safety", "evt1")
    tmp_logger.log("safety", "evt2")
    assert tmp_logger.dropped_count == 2


def test_on_drop_callback_invoked():
    import shutil
    fired: list = []
    with tempfile.TemporaryDirectory() as td:
        logger = AuditLogger(log_dir=td, robot_id="r1",
                             on_drop=lambda e, exc: fired.append((e, exc)))
        shutil.rmtree(td)
        logger.log("safety", "evt1")
    assert len(fired) == 1
    entry, exc = fired[0]
    assert entry["event"] == "evt1"
    assert isinstance(exc, OSError)


def test_prev_hash_does_not_advance_on_write_failure(tmp_logger):
    """If a write fails, the next entry must still link to the previous
    successful entry — not to a self_hash that never landed."""
    h1 = tmp_logger.log("safety", "ok1")
    # Force a write failure on the next call by clobbering the log dir.
    import shutil
    shutil.rmtree(tmp_logger.log_dir)
    failed = tmp_logger.log("safety", "would_fail")
    assert failed == ""             # signal: write didn't reach disk
    assert tmp_logger._prev_hash == h1   # chain head unchanged
    assert tmp_logger.dropped_count == 1


# ─── Rotation-aware verify_chain_all (H4 fix) ───
def test_verify_chain_all_across_rotations(tmp_logger):
    """A 2-file chain (manually rotated) must verify end-to-end."""
    tmp_logger.log("safety", "evt_1")
    tmp_logger.log("safety", "evt_2")
    # Manually rotate: close current file and start a new one.
    tmp_logger._current_file = (
        Path(tmp_logger.log_dir) / "20260101_1.jsonl")
    tmp_logger._current_size = 0
    tmp_logger.log("safety", "evt_3")
    tmp_logger.log("safety", "evt_4")
    valid, bad_file, bad_line = tmp_logger.verify_chain_all()
    assert valid is True, f"chain broke at {bad_file}:{bad_line}"
    assert bad_file is None


def test_verify_chain_all_detects_tampering_in_rotated_file(tmp_logger):
    tmp_logger.log("safety", "evt_1")
    tmp_logger._current_file = (
        Path(tmp_logger.log_dir) / "20260101_1.jsonl")
    tmp_logger._current_size = 0
    tmp_logger.log("safety", "evt_2")
    # Tamper with the second file.
    files = sorted(Path(tmp_logger.log_dir).glob("*.jsonl"))
    second = files[-1]
    e = json.loads(second.read_text().strip())
    e["params"] = {"tampered": True}
    second.write_text(json.dumps(e, sort_keys=True) + "\n")
    valid, bad_file, bad_line = tmp_logger.verify_chain_all()
    assert valid is False
    assert bad_file == second
    assert bad_line == 1


# ─── Partial tail truncation on restart (C2 fix) ───
def test_resume_truncates_partial_tail_line(tmp_logger):
    """If the previous process crashed mid-write, the restart path must
    truncate the partial line and continue from the last good hash —
    not fall back to GENESIS and silently break the chain."""
    tmp_logger.log("safety", "evt_1")
    h2 = tmp_logger.log("safety", "evt_2")
    files = sorted(Path(tmp_logger.log_dir).glob("*.jsonl"))
    # Append a half-written line (no trailing newline, truncated JSON).
    with files[0].open("ab") as f:
        f.write(b'{"category":"safety","event":"crashed_mid_wri')
    # Fresh logger — should detect + truncate the partial line.
    logger2 = AuditLogger(log_dir=str(tmp_logger.log_dir),
                          robot_id="robot-test")
    assert logger2._prev_hash == h2       # last good hash, not GENESIS
    logger2.log("safety", "evt_3")
    valid, _bf, _bl = logger2.verify_chain_all()
    assert valid is True
