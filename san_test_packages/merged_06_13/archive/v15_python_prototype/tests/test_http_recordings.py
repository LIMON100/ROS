"""
End-to-end tests for HttpRecordingsProcess.

Real ThreadingHTTPServer on localhost; tests use urllib to issue
catalog/file/Range requests and verify the responses.
"""
from __future__ import annotations

import json
import multiprocessing as mp
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from control.http_recordings_process import (
    _RECORDING_RE,
    HttpRecordingsProcess,
    _parse_range,
)

_PORT = 26000


def _make_proc(rec_dir: Path, port: int):
    cfg = MagicMock()
    cfg.get.side_effect = lambda *k, default=None: {
        ("http", "port"):              port,
        ("http", "bind"):              "127.0.0.1",
        ("http", "recordings_dir"):    str(rec_dir),
        ("system", "cpu_affinity", "comm"): None,
    }.get(tuple(k), default)

    p = HttpRecordingsProcess.__new__(HttpRecordingsProcess)
    p.queues = MagicMock()
    p.shutdown_event = mp.Event()
    p.cfg = cfg
    p.log = MagicMock()
    p._server = None
    p._server_thread = None
    p._stats = {"requests": 0}
    p.spawn_thread = lambda target, name: threading.Thread(
        target=target, name=name, daemon=True).start()
    p.is_running = lambda: not p.shutdown_event.is_set()
    p.setup()
    time.sleep(0.3)
    return p


@pytest.fixture
def proc(tmp_path):
    global _PORT
    _PORT += 1
    p = _make_proc(tmp_path, _PORT)
    yield p, tmp_path, _PORT
    p.shutdown_event.set()
    p.teardown()


# ════════════════════════════════════════════════════════════════
# _parse_range — pure-function tests
# ════════════════════════════════════════════════════════════════
def test_parse_range_no_header_returns_none():
    assert _parse_range(None, 1000) == (None, None)


def test_parse_range_full_range():
    assert _parse_range("bytes=0-999", 1000) == (0, 999)


def test_parse_range_open_end_clipped():
    assert _parse_range("bytes=500-", 1000) == (500, 999)


def test_parse_range_suffix():
    assert _parse_range("bytes=-100", 1000) == (900, 999)


def test_parse_range_unsatisfiable_start_past_eof():
    assert _parse_range("bytes=2000-3000", 1000) == (None, None)


def test_parse_range_invalid_syntax():
    assert _parse_range("bytes=abc", 1000) == (None, None)
    assert _parse_range("items=0-100", 1000) == (None, None)


# ════════════════════════════════════════════════════════════════
# /recordings catalog
# ════════════════════════════════════════════════════════════════
def test_catalog_empty_when_dir_empty(proc):
    _, _, port = proc
    with urllib.request.urlopen(f"http://127.0.0.1:{port}/recordings",
                                  timeout=2.0) as r:
        assert r.status == 200
        assert json.loads(r.read()) == []


def test_catalog_lists_only_recording_files(proc):
    _, rec_dir, port = proc
    (rec_dir / "patrol_20260509.mp4").write_bytes(b"x" * 100)
    (rec_dir / "stray_log.txt").write_bytes(b"y")        # not a recording
    (rec_dir / "name with space.mp4").write_bytes(b"z")  # invalid name (regex)

    with urllib.request.urlopen(f"http://127.0.0.1:{port}/recordings",
                                  timeout=2.0) as r:
        data = json.loads(r.read())
    names = [d["name"] for d in data]
    assert "patrol_20260509.mp4" in names
    assert "stray_log.txt" not in names
    # Names with whitespace fail the regex even if they exist on disk
    assert "name with space.mp4" not in names


def test_catalog_includes_size_and_url(proc):
    _, rec_dir, port = proc
    (rec_dir / "test.mp4").write_bytes(b"x" * 256)

    with urllib.request.urlopen(f"http://127.0.0.1:{port}/recordings",
                                  timeout=2.0) as r:
        data = json.loads(r.read())
    entry = next(d for d in data if d["name"] == "test.mp4")
    assert entry["size"] == 256
    assert entry["url"] == "/recordings/test.mp4"


# ════════════════════════════════════════════════════════════════
# /recordings/<name> — full GET
# ════════════════════════════════════════════════════════════════
def test_get_full_file_returns_200(proc):
    _, rec_dir, port = proc
    body = b"hello world" * 100
    (rec_dir / "x.mp4").write_bytes(body)

    with urllib.request.urlopen(f"http://127.0.0.1:{port}/recordings/x.mp4",
                                  timeout=2.0) as r:
        assert r.status == 200
        assert r.headers.get("Accept-Ranges") == "bytes"
        assert r.read() == body


def test_get_unknown_file_404(proc):
    _, _, port = proc
    with pytest.raises(urllib.error.HTTPError) as exc:
        urllib.request.urlopen(f"http://127.0.0.1:{port}/recordings/missing.mp4",
                                 timeout=2.0)
    assert exc.value.code == 404


def test_get_invalid_filename_400(proc):
    _, _, port = proc
    # Path traversal attempt
    with pytest.raises(urllib.error.HTTPError) as exc:
        urllib.request.urlopen(
            f"http://127.0.0.1:{port}/recordings/..%2Fetc%2Fpasswd",
            timeout=2.0)
    assert exc.value.code in (400, 404)


# ════════════════════════════════════════════════════════════════
# Range-GET (RFC 7233)
# ════════════════════════════════════════════════════════════════
def test_range_get_returns_206(proc):
    _, rec_dir, port = proc
    body = bytes(range(256)) * 4        # 1024 bytes
    (rec_dir / "x.mp4").write_bytes(body)

    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/recordings/x.mp4",
        headers={"Range": "bytes=10-19"})
    with urllib.request.urlopen(req, timeout=2.0) as r:
        assert r.status == 206
        assert r.headers["Content-Range"] == "bytes 10-19/1024"
        assert r.headers["Content-Length"] == "10"
        assert r.read() == body[10:20]


def test_range_get_open_end(proc):
    _, rec_dir, port = proc
    body = b"a" * 500 + b"b" * 500
    (rec_dir / "x.mp4").write_bytes(body)

    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/recordings/x.mp4",
        headers={"Range": "bytes=500-"})
    with urllib.request.urlopen(req, timeout=2.0) as r:
        assert r.status == 206
        assert r.read() == b"b" * 500


def test_range_get_suffix(proc):
    _, rec_dir, port = proc
    body = b"a" * 900 + b"z" * 100
    (rec_dir / "x.mp4").write_bytes(body)

    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/recordings/x.mp4",
        headers={"Range": "bytes=-100"})
    with urllib.request.urlopen(req, timeout=2.0) as r:
        assert r.status == 206
        assert r.read() == b"z" * 100


# ════════════════════════════════════════════════════════════════
# DELETE
# ════════════════════════════════════════════════════════════════
def test_delete_removes_file(proc):
    _, rec_dir, port = proc
    target = rec_dir / "del_me.mp4"
    target.write_bytes(b"x")
    assert target.exists()

    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/recordings/del_me.mp4",
        method="DELETE")
    with urllib.request.urlopen(req, timeout=2.0) as r:
        assert r.status == 204
    assert not target.exists()


def test_delete_unknown_404(proc):
    _, _, port = proc
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/recordings/missing.mp4",
        method="DELETE")
    with pytest.raises(urllib.error.HTTPError) as exc:
        urllib.request.urlopen(req, timeout=2.0)
    assert exc.value.code == 404


# ════════════════════════════════════════════════════════════════
# /healthz
# ════════════════════════════════════════════════════════════════
def test_healthz_returns_200(proc):
    _, _, port = proc
    with urllib.request.urlopen(f"http://127.0.0.1:{port}/healthz",
                                  timeout=2.0) as r:
        assert r.status == 200
        assert r.read() == b"ok\n"


# ════════════════════════════════════════════════════════════════
# Filename validation
# ════════════════════════════════════════════════════════════════
def test_recording_re_accepts_typical_names():
    assert _RECORDING_RE.match("patrol_20260509_120000.mp4")
    assert _RECORDING_RE.match("recon-001.h265")


def test_recording_re_rejects_traversal():
    assert _RECORDING_RE.match("../../etc/passwd") is None
    assert _RECORDING_RE.match("name with space.mp4") is None
    assert _RECORDING_RE.match("..mp4") is None
