"""
Tests for core.diag — logging, metrics, crash handler, tracing.

These run without spawning child processes — diag is designed to work
in-process so the same APIs that BaseProcess wraps can be exercised here.
"""
from __future__ import annotations

import json
import os
import threading
import time
from pathlib import Path

import pytest

from core.diag import (
    MetricsCollector,
    _gather_runtime_state,
    _write_crash,
    get_trace_buffer,
    install_crash_handler,
    set_correlation_id,
    set_tracing,
    setup_logger,
    trace,
)


# ════════════════════════════════════════════════════════════════
# Logging
# ════════════════════════════════════════════════════════════════
def test_setup_logger_writes_to_file(tmp_path):
    log = setup_logger("test_log_writes", log_dir=tmp_path)
    log.info("hello world")
    # Force flush
    for h in log.handlers:
        h.flush()
    files = list(tmp_path.glob("test_log_writes.log*"))
    assert len(files) == 1
    text = files[0].read_text()
    assert "hello world" in text


def test_setup_logger_idempotent(tmp_path):
    """Calling setup twice doesn't multiply handlers."""
    log = setup_logger("test_idempotent", log_dir=tmp_path)
    log = setup_logger("test_idempotent", log_dir=tmp_path)
    # Should still have exactly 3 patrol-managed handlers
    # (stream + file + recent-logs ring for crash dumps)
    handlers = [h for h in log.handlers
                if getattr(h, "_patrol_managed", False)]
    assert len(handlers) == 3


def test_setup_logger_json_format_emits_valid_json(tmp_path, caplog):
    log = setup_logger("test_json", log_dir=tmp_path, json_format=True)
    log.info("structured msg", extra={"ext_key": "value", "ext_n": 42})
    for h in log.handlers:
        h.flush()
    text = (tmp_path / "test_json.log").read_text()
    line = text.strip().split("\n")[-1]
    parsed = json.loads(line)
    assert parsed["msg"] == "structured msg"
    assert parsed["key"] == "value"
    assert parsed["n"] == 42
    assert parsed["level"] == "INFO"


def test_setup_logger_includes_correlation_id(tmp_path):
    set_correlation_id("test_proc:1234")
    log = setup_logger("test_cid", log_dir=tmp_path, json_format=True)
    log.info("hi")
    for h in log.handlers:
        h.flush()
    line = (tmp_path / "test_cid.log").read_text().strip().split("\n")[-1]
    assert json.loads(line)["cid"] == "test_proc:1234"
    set_correlation_id("")        # don't leak into other tests


# ════════════════════════════════════════════════════════════════
# MetricsCollector
# ════════════════════════════════════════════════════════════════
def test_metrics_records_step_latency():
    m = MetricsCollector("unit")
    for i in range(10):
        m.step_begin()
        # Shift t0 back to simulate deterministic latency — avoids
        # flakiness from Windows timer granularity (~15.6 ms).
        m._step_t0 -= 0.005 * (i + 1)
        m.step_end()
    s = m.snapshot()
    assert s.step_count == 10
    assert len(s.latency_samples_ms) == 10
    assert all(x > 0 for x in s.latency_samples_ms)


def test_metrics_percentiles_are_monotone():
    m = MetricsCollector("unit")
    # Synthetic samples 1..100
    for ms in range(1, 101):
        m.step_begin()
        m._step_t0 = time.monotonic() - ms / 1000.0
        m.step_end()
    s = m.snapshot()
    p50 = s.percentile(50)
    p95 = s.percentile(95)
    p99 = s.percentile(99)
    assert p50 < p95 <= p99
    # p99 should be near the top of the range
    assert p99 >= 90.0


def test_metrics_reservoir_caps_at_256():
    m = MetricsCollector("unit")
    for _ in range(500):
        m.step_begin()
        m.step_end()
    s = m.snapshot()
    assert len(s.latency_samples_ms) == 256


def test_metrics_increment_counters_atomic():
    """Concurrent increments must not lose updates."""
    m = MetricsCollector("unit")
    N = 1000

    def worker():
        for _ in range(N):
            m.increment("hit")

    ts = [threading.Thread(target=worker) for _ in range(4)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    assert m.snapshot().counters["hit"] == 4 * N


def test_metrics_record_exception_captures_type_and_message():
    m = MetricsCollector("unit")
    try:
        raise ValueError("boom")
    except ValueError as e:
        m.record_exception(e)
    s = m.snapshot()
    assert s.exception_count == 1
    assert "ValueError" in s.last_exception
    assert "boom" in s.last_exception


def test_metrics_publishes_to_shared_dict():
    """Snapshot is mirrored into the shared dict at publish_period_s intervals."""
    shared = {}
    m = MetricsCollector("publisher", shared_dict=shared,
                         publish_period_s=0.0)   # publish every step
    for _ in range(5):
        m.step_begin()
        m.step_end()
    assert "publisher" in shared
    assert shared["publisher"]["name"] == "publisher"
    assert shared["publisher"]["step_count"] == 5


def test_metrics_healthy_flag_reflects_recent_steps():
    m = MetricsCollector("unit")
    assert m.snapshot().healthy(max_step_age_s=5.0) is True   # never stepped
    m.step_begin()
    m.step_end()
    assert m.snapshot().healthy(max_step_age_s=5.0) is True
    # Simulate an aged last_step_at
    m._m.last_step_at = time.monotonic() - 10.0
    assert m.snapshot().healthy(max_step_age_s=5.0) is False


# ════════════════════════════════════════════════════════════════
# Crash handler
# ════════════════════════════════════════════════════════════════
def test_crash_handler_writes_dump_with_exception(tmp_path):
    log = setup_logger("crash_test", log_dir=tmp_path)
    install_crash_handler(tmp_path / "crashes", logger=log)
    try:
        raise RuntimeError("intentional")
    except RuntimeError as e:
        path = _write_crash("test_event", exc=e, process_name="unit_test_proc")
    assert path is not None
    assert path.exists()
    data = json.loads(path.read_text())
    assert data["reason"] == "test_event"
    assert data["exception"]["type"] == "RuntimeError"
    assert "intentional" in data["exception"]["msg"]
    assert "test_diag" in data["exception"]["traceback"]
    # Runtime context captured
    assert data["pid"] == os.getpid()
    assert "stacks" in data and isinstance(data["stacks"], dict)


def test_crash_handler_includes_recent_logs(tmp_path):
    log = setup_logger("crash_logs_test", log_dir=tmp_path)
    install_crash_handler(tmp_path / "crashes", logger=log)
    log.warning("preceding event 1")
    log.error("preceding event 2")
    path = _write_crash("manual_dump", process_name="unit_test_proc")
    assert path is not None
    data = json.loads(path.read_text())
    logs = data["recent_logs"]
    assert any("preceding event 1" in line for line in logs)
    assert any("preceding event 2" in line for line in logs)


def test_gather_runtime_state_has_required_keys():
    state = _gather_runtime_state()
    assert state["pid"] == os.getpid()
    assert "thread_count" in state
    assert "stacks" in state
    assert state["thread_count"] >= 1
    # Main thread should appear
    assert any(t["name"] == "MainThread" for t in state["threads"])


# ════════════════════════════════════════════════════════════════
# Tracing
# ════════════════════════════════════════════════════════════════
def test_trace_zero_cost_when_disabled():
    set_tracing(False)
    before = len(get_trace_buffer())
    for _ in range(100):
        trace("noisy_event", v=42)
    assert len(get_trace_buffer()) == before


def test_trace_records_when_enabled():
    set_tracing(True)
    try:
        # Note: get_trace_buffer is global, so other tests may have added entries
        before = len(get_trace_buffer())
        trace("loc_pose", source="rtk_fixed", sigma=0.02)
        trace("loc_pose", source="odometry",  sigma=0.50)
        buf = get_trace_buffer()
        assert len(buf) >= before + 2
        last2 = buf[-2:]
        assert all(e["ev"] == "loc_pose" for e in last2)
        assert last2[0]["source"] == "rtk_fixed"
        assert last2[1]["sigma"] == 0.50
    finally:
        set_tracing(False)


# ════════════════════════════════════════════════════════════════
# Profiler
# ════════════════════════════════════════════════════════════════
def test_profiler_writes_pstats_on_flush(tmp_path):
    from core.diag import _StepProfiler
    p = _StepProfiler(dump_dir=tmp_path, process_name="prof_test")
    p.enable()

    # Do something measurable
    def busy():
        s = 0
        for i in range(100_000):
            s += i
        return s
    busy()

    path = p.disable_and_flush()
    assert path is not None
    assert path.exists()
    assert path.suffix == ".pstats"
    # Verify we can read it back with pstats
    import pstats
    stats = pstats.Stats(str(path))
    assert stats.total_calls > 0


def test_profiler_periodic_dump(tmp_path):
    from core.diag import _StepProfiler
    p = _StepProfiler(dump_dir=tmp_path, process_name="periodic",
                      dump_period_s=0.0)   # always dump
    p.enable()
    sum(range(1000))
    path = p.maybe_dump()
    assert path is not None and path.exists()
    p.disable_and_flush()


def test_profiler_disable_when_not_enabled_is_safe():
    from core.diag import _StepProfiler
    p = _StepProfiler(dump_dir=Path("/tmp"), process_name="x")
    # Never enabled — disable_and_flush should be a no-op
    assert p.disable_and_flush() is None


# ════════════════════════════════════════════════════════════════
# Hang detector
# ════════════════════════════════════════════════════════════════
def test_hang_detector_does_not_fire_when_disarmed_in_time(tmp_path):
    from core.diag import HangDetector, install_crash_handler

    install_crash_handler(tmp_path)
    h = HangDetector("nonblocking", timeout_s=0.5)
    h.arm()
    time.sleep(0.05)        # well within timeout
    h.disarm()
    time.sleep(0.6)         # past the original deadline
    # No crash dump should have been written
    crashes = list(tmp_path.glob("nonblocking_*hang*.json"))
    assert len(crashes) == 0


def test_hang_detector_fires_when_step_takes_too_long(tmp_path):
    from core.diag import HangDetector, install_crash_handler

    install_crash_handler(tmp_path)
    h = HangDetector("blocking", timeout_s=0.1)
    h.arm()
    time.sleep(0.3)         # exceeds timeout — timer fires
    # Don't disarm — let the timer fire
    time.sleep(0.05)        # let the dump complete
    crashes = list(tmp_path.glob("blocking_*hang_blocking*.json"))
    assert len(crashes) >= 1
    data = json.loads(crashes[0].read_text())
    assert data["exception"]["type"] == "TimeoutError"


def test_hang_detector_zero_timeout_is_disabled():
    from core.diag import HangDetector
    h = HangDetector("disabled", timeout_s=0.0)
    h.arm()             # should be a no-op
    assert h._timer is None


# ════════════════════════════════════════════════════════════════
# Schema validation
# ════════════════════════════════════════════════════════════════
def test_schema_check_passes_for_well_formed_message():
    import numpy as np

    from core.diag import set_schema_check, validate_message
    from core.messages import Header, RtkFix
    set_schema_check(True)
    try:
        msg = RtkFix(
            header=Header.now(frame_id="map"),
            lat=37.5, lon=127.0, alt=50.0,
            enu=np.zeros(3, dtype=np.float32),
            fix_quality=4, n_satellites=18,
            hdop=0.6, sigma_xy=0.02, sigma_z=0.03,
        )
        validate_message(msg)        # no raise
    finally:
        set_schema_check(False)


def test_schema_check_catches_wrong_field_type():
    from core.diag import set_schema_check, validate_message
    from core.messages import Header, RtkFix
    set_schema_check(True)
    try:
        bad = RtkFix.__new__(RtkFix)
        # Bypass __init__ to construct an invalid instance
        bad.header = Header.now()
        bad.lat = "not a float"          # ← wrong type
        bad.lon = 0.0
        bad.alt = 0.0
        import numpy as np
        bad.enu = np.zeros(3, dtype=np.float32)
        bad.fix_quality = 0
        bad.n_satellites = 0
        bad.hdop = 0.0
        bad.sigma_xy = 0.0
        bad.sigma_z = 0.0
        with pytest.raises(RuntimeError, match="lat"):
            validate_message(bad)
    finally:
        set_schema_check(False)


def test_schema_check_disabled_zero_cost():
    """When disabled, validate_message returns immediately without inspection."""
    from core.diag import schema_check_enabled, set_schema_check, validate_message
    set_schema_check(False)
    assert not schema_check_enabled()
    # Pass nonsense — should NOT raise
    validate_message(object())          # not even a dataclass
    validate_message(None)
    validate_message(42)


def test_schema_check_skips_non_dataclass():
    """Numpy arrays and primitives flow through unvalidated."""
    import numpy as np

    from core.diag import set_schema_check, validate_message
    set_schema_check(True)
    try:
        validate_message(np.zeros(3))   # not a dataclass — no raise
        validate_message(b"raw bytes")
        validate_message({"key": "value"})
    finally:
        set_schema_check(False)


def test_schema_check_caches_field_resolution():
    """Repeated validate_message on same type must reuse cached field list."""
    from core.diag import _SCHEMA_FIELDS_CACHE, set_schema_check, validate_message
    from core.messages import Header, LrfReading
    set_schema_check(True)
    try:
        _SCHEMA_FIELDS_CACHE.clear()
        msg = LrfReading(header=Header.now(), range_m=5.0,
                         return_strength=0.9, valid=True)
        validate_message(msg)
        validate_message(msg)
        validate_message(msg)
        # After three validations, cache has exactly one entry for LrfReading
        assert LrfReading in _SCHEMA_FIELDS_CACHE
    finally:
        set_schema_check(False)
