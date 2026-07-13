"""
HTTP uploader tests for CommProcess.

Two flavors:
  • Unit tests with mocked urllib — verify payload shape, retries, error handling.
  • Live test against an actual uvicorn-spawned patrol_server — only runs
    if PATROL_SERVER_LIVE=1 in the env. Skipped by default in CI.

The unit tests don't spawn a CommProcess — they call its methods on an
in-process instance built via __new__, which is reasonable here because
all the wire-layer code is independent of multiprocessing.
"""
from __future__ import annotations

import json
import socket
import threading
import urllib.error
import urllib.request
from unittest.mock import MagicMock, patch

import numpy as np
import pytest

from comm.comm_process import CommProcess, _anomaly_to_payload, _pose_to_list
from core.messages import (
    AnomalyEvent,
    Detection,
    Header,
    Pose6D,
)


def _make_comm() -> CommProcess:
    """Build a CommProcess instance without spawning. Suitable for unit tests
    that exercise the wire layer (HTTP) without IPC."""
    c = CommProcess.__new__(CommProcess)
    c.log = MagicMock()
    c._server_url = "http://localhost:9000"
    c._robot_id = "go2-test"
    c._link_lock = threading.Lock()
    c._state_lock = threading.Lock()
    c._active_link = "wifi6"
    c._latest_pose = None
    c._latest_status = None
    c._latest_rtk_quality = 0
    c._lte_status = None
    c._stats = {"uploaded_wifi": 0, "uploaded_lte": 0,
                "cached": 0, "heartbeat_ok": 0, "heartbeat_fail": 0,
                "link_switches": 0}
    return c


# ════════════════════════════════════════════════════════════════
# _pose_to_list / _anomaly_to_payload — pure transforms
# ════════════════════════════════════════════════════════════════
def test_pose_to_list_none_returns_none():
    assert _pose_to_list(None) is None


def test_pose_to_list_serializes_position_and_orientation():
    p = Pose6D(
        header=Header.now(frame_id="map"),
        position=np.array([1.5, 2.5, 0.5], dtype=np.float32),
        orientation=np.array([0, 0, 0.707, 0.707], dtype=np.float32),
    )
    out = _pose_to_list(p)
    assert out == [pytest.approx(1.5), pytest.approx(2.5), pytest.approx(0.5),
                   pytest.approx(0.0), pytest.approx(0.0),
                   pytest.approx(0.707), pytest.approx(0.707)]


def test_anomaly_to_payload_basic_shape():
    ev = AnomalyEvent(
        header=Header.now(frame_id="map"),
        severity="warning", category="detection",
        description="person×1",
        detections=[Detection(
            label="person", confidence=0.9,
            bbox=np.array([10, 20, 30, 40], dtype=np.float32),
            pose_at_detect=None,
        )],
        image_ref=None,
    )
    payload = _anomaly_to_payload("go2-1", ev, include_image=True)
    assert payload["robot_id"] == "go2-1"
    assert payload["severity"] == "warning"
    assert payload["category"] == "detection"
    assert len(payload["detections"]) == 1
    d = payload["detections"][0]
    assert d["label"] == "person"
    assert d["bbox"] == [10.0, 20.0, 30.0, 40.0]
    assert d["confidence"] == pytest.approx(0.9)


def test_anomaly_to_payload_strips_image_when_lte():
    """LTE path: image_ref must not balloon the payload."""
    ev = AnomalyEvent(
        header=Header.now(frame_id="map"), severity="info",
        category="detection", description="x", detections=[],
        image_ref=None,
    )
    payload = _anomaly_to_payload("go2-1", ev, include_image=False)
    assert "image_url" not in payload


# ════════════════════════════════════════════════════════════════
# _post_json — HTTP behavior with mocked urllib
# ════════════════════════════════════════════════════════════════
class _FakeResp:
    def __init__(self, status: int):
        self.status = status
    def __enter__(self): return self
    def __exit__(self, *a): pass


def test_post_json_returns_true_on_2xx():
    c = _make_comm()
    with patch("urllib.request.urlopen", return_value=_FakeResp(204)) as m:
        ok = c._post_json("/api/v1/heartbeat", {"x": 1})
    assert ok is True
    # Verify request was constructed correctly
    req = m.call_args.args[0]
    assert req.full_url.endswith("/api/v1/heartbeat")
    assert req.headers["Content-type"] == "application/json"
    assert json.loads(req.data) == {"x": 1}


def test_post_json_returns_false_on_4xx():
    c = _make_comm()
    err = urllib.error.HTTPError(
        url="http://x", code=422, msg="Unprocessable", hdrs={}, fp=None)
    with patch("urllib.request.urlopen", side_effect=err):
        ok = c._post_json("/api/v1/heartbeat", {})
    assert ok is False
    c.log.warning.assert_called()


def test_post_json_returns_false_on_unreachable():
    c = _make_comm()
    err = urllib.error.URLError("connection refused")
    with patch("urllib.request.urlopen", side_effect=err):
        ok = c._post_json("/api/v1/heartbeat", {})
    assert ok is False


def test_post_json_returns_false_on_timeout():
    c = _make_comm()
    with patch("urllib.request.urlopen", side_effect=socket.timeout("slow")):
        ok = c._post_json("/api/v1/heartbeat", {})
    assert ok is False


def test_post_json_returns_false_when_payload_unserializable():
    """Caller bug: passing a non-JSON object should fail-loud, not raise."""
    c = _make_comm()
    bad = {"obj": object()}
    with patch("urllib.request.urlopen") as m:
        ok = c._post_json("/api/v1/heartbeat", bad)
    assert ok is False
    m.assert_not_called()
    c.log.error.assert_called()


def test_post_json_returns_false_when_server_url_empty():
    c = _make_comm()
    c._server_url = ""
    with patch("urllib.request.urlopen") as m:
        ok = c._post_json("/api/v1/heartbeat", {})
    assert ok is False
    m.assert_not_called()


# ════════════════════════════════════════════════════════════════
# _send_heartbeat — payload assembly + POST
# ════════════════════════════════════════════════════════════════
def test_heartbeat_uses_latest_status_and_link():
    c = _make_comm()
    c._active_link = "lte"
    c._latest_rtk_quality = 4
    # Synthetic RobotStatus with battery_soc
    from core.messages import RobotStatus
    c._latest_status = RobotStatus(
        header=Header.now(),
        battery_soc=0.42, motor_temp_max=35.0,
        locomotion_mode="walk", fault_codes=(),
    )

    captured = {}
    def fake_post(path, payload, timeout_s=3.0):
        captured["path"] = path
        captured["payload"] = payload
        return True

    with patch.object(c, "_post_json", side_effect=fake_post):
        ok = c._send_heartbeat()

    assert ok is True
    assert captured["path"] == "/api/v1/heartbeat"
    p = captured["payload"]
    assert p["robot_id"] == "go2-test"
    assert p["battery_soc"] == pytest.approx(0.42)
    assert p["active_link"] == "lte"
    assert p["rtk_quality"] == 4
    assert "metrics" in p


def test_heartbeat_safe_when_no_status_yet():
    """Boot ordering: heartbeat thread fires before status arrives.
    Must not crash; must default to battery=1.0."""
    c = _make_comm()
    with patch.object(c, "_post_json", return_value=True) as m:
        c._send_heartbeat()
    payload = m.call_args.args[1]
    assert payload["battery_soc"] == 1.0
    assert payload["pose"] is None


# ════════════════════════════════════════════════════════════════
# _upload_anomaly — converts and posts
# ════════════════════════════════════════════════════════════════
def test_upload_anomaly_strips_image_on_lte():
    c = _make_comm()
    ev = AnomalyEvent(
        header=Header.now(), severity="warning", category="detection",
        description="x", detections=[], image_ref=None,
    )
    captured = {}
    def fake_post(path, payload, timeout_s=3.0):
        captured["payload"] = payload
        return True
    with patch.object(c, "_post_json", side_effect=fake_post):
        c._upload_anomaly(ev, via="lte")
    assert "image_url" not in captured["payload"]


def test_upload_anomaly_returns_false_on_serialization_error():
    """Bad ev (e.g. None header) should not raise."""
    c = _make_comm()
    bad = MagicMock()
    bad.detections = []
    bad.header = None    # → AttributeError when serializing
    ok = c._upload_anomaly(bad, via="wifi6")
    assert ok is False


# ════════════════════════════════════════════════════════════════
# _upload_crash_dump — file → server schema
# ════════════════════════════════════════════════════════════════
def test_upload_crash_dump_translates_diag_format(tmp_path):
    c = _make_comm()
    # Simulate a diag.py crash dump file
    crash = {
        "reason": "hang_Localization",
        "process": "Localization",
        "pid": 1234,
        "wall_time": "2026-05-09T10:00:00",
        "exception": {
            "type": "TimeoutError",
            "msg": "step() exceeded 5.0s",
            "traceback": "File foo.py line 42\n  ...",
        },
        "recent_logs": [f"line {i}" for i in range(300)],   # over the 200 cap
        "proc": {"VmRSS": "120000 kB", "Threads": "8"},
    }
    path = tmp_path / "Localization_20260509_hang.json"
    path.write_text(json.dumps(crash))

    captured = {}
    def fake_post(p, payload, timeout_s=3.0):
        captured["path"] = p
        captured["payload"] = payload
        return True
    with patch.object(c, "_post_json", side_effect=fake_post):
        ok = c._upload_crash_dump(path, via="wifi6")

    assert ok is True
    assert captured["path"] == "/api/v1/crashes"
    p = captured["payload"]
    assert p["robot_id"] == "go2-test"
    assert p["process"] == "Localization"
    assert p["reason"] == "hang_Localization"
    assert p["exception_type"] == "TimeoutError"
    assert p["exception_msg"] == "step() exceeded 5.0s"
    assert "File foo.py" in p["traceback"]
    # Recent logs capped at 200 (not 300)
    assert len(p["recent_logs"]) == 200
    assert p["proc_state"]["Threads"] == "8"


def test_upload_crash_dump_unreadable_file_returns_false(tmp_path):
    c = _make_comm()
    # File doesn't exist
    ok = c._upload_crash_dump(tmp_path / "missing.json", via="wifi6")
    assert ok is False


def test_upload_crash_dump_corrupt_json_returns_false(tmp_path):
    c = _make_comm()
    p = tmp_path / "broken.json"
    p.write_text("{not valid json")
    ok = c._upload_crash_dump(p, via="wifi6")
    assert ok is False


# ════════════════════════════════════════════════════════════════
# _cache_to_disk — persistence for retry
# ════════════════════════════════════════════════════════════════
def test_cache_to_disk_writes_atomic_file(tmp_path):
    c = _make_comm()
    c._cache_dir = tmp_path
    ev = AnomalyEvent(
        header=Header.now(), severity="info", category="detection",
        description="x", detections=[], image_ref=None,
    )
    c._cache_to_disk(ev)
    files = list(tmp_path.glob("anomaly_*.bin"))
    assert len(files) == 1
    # Half-written tmp files must not survive
    tmps = list(tmp_path.glob(".anomaly_*.tmp"))
    assert tmps == []
    # Content is valid JSON matching server schema
    payload = json.loads(files[0].read_text())
    assert payload["robot_id"] == "go2-test"
    assert payload["category"] == "detection"


# ════════════════════════════════════════════════════════════════
# Live integration — robot ↔ server (skipped by default)
# ════════════════════════════════════════════════════════════════
# ════════════════════════════════════════════════════════════════
# WS-redirect mode (Patrol Server disabled)
# ════════════════════════════════════════════════════════════════
class _StubQueue:
    def __init__(self):
        self.items = []
    def put_nowait(self, x): self.items.append(x)
    def get_nowait(self):
        if not self.items:
            raise __import__("queue").Empty
        return self.items.pop(0)


def test_ws_anomaly_broadcast_serializes_event_without_image():
    c = _make_comm()
    c.queues = MagicMock()
    c.queues.ws_anomaly = _StubQueue()
    ev = AnomalyEvent(
        header=Header.now(), severity="warning", category="detection",
        description="person×1",
        detections=[Detection(label="person", confidence=0.9,
                              bbox=np.array([10, 20, 30, 40], dtype=np.float32),
                              pose_at_detect=None)],
        image_ref=None,
    )
    c._broadcast_anomaly_ws(ev)
    assert len(c.queues.ws_anomaly.items) == 1
    payload = c.queues.ws_anomaly.items[0]
    assert payload["robot_id"] == "go2-test"
    assert payload["category"] == "detection"
    assert "image_url" not in payload


def test_ws_heartbeat_broadcast_publishes_snapshot_dict():
    from core.messages import RobotStatus
    c = _make_comm()
    c.queues = MagicMock()
    c.queues.ws_heartbeat = _StubQueue()
    c._latest_status = RobotStatus(
        header=Header.now(), battery_soc=0.55, motor_temp_max=33.0,
        locomotion_mode="walk", fault_codes=())
    c._latest_rtk_quality = 4
    ok = c._broadcast_heartbeat_ws()
    assert ok is True
    assert len(c.queues.ws_heartbeat.items) == 1
    p = c.queues.ws_heartbeat.items[0]
    assert p["robot_id"] == "go2-test"
    assert p["battery_soc"] == pytest.approx(0.55)
    assert p["active_link"] == "ws"
    assert p["rtk_quality"] == 4


def test_ws_anomaly_broadcast_swallows_serialization_error():
    """A bad event must not raise — debug log only."""
    c = _make_comm()
    c.queues = MagicMock()
    c.queues.ws_anomaly = _StubQueue()
    bad = MagicMock()
    bad.detections = []
    bad.header = None
    c._broadcast_anomaly_ws(bad)
    # Nothing pushed, no exception
    assert c.queues.ws_anomaly.items == []


@pytest.mark.skip(
    reason="Patrol Server deferred to Phase F per Rev.A.5 — "
           "set PATROL_SERVER_LIVE=1 once a server is reachable on :9000")
def test_live_heartbeat_round_trip():
    """End-to-end: a real urllib POST to a real running uvicorn instance."""
    c = _make_comm()
    c._server_url = "http://localhost:9000"
    ok = c._send_heartbeat()
    assert ok, "live server not reachable on :9000"
    # Verify the dashboard now lists go2-test
    with urllib.request.urlopen("http://localhost:9000/api/v1/robots") as r:
        data = json.loads(r.read())
    assert any(row["robot_id"] == "go2-test" for row in data)
