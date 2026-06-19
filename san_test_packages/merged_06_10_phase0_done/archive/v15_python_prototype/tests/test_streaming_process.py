"""
Tests for StreamingProcess.

Pipeline builder is pure → easy to unit test exhaustively.
Subprocess lifecycle uses a real /bin/sh stub so we don't need GStreamer
installed; the lifecycle paths (start/stop/crash detection) are still
exercised end-to-end.
"""
from __future__ import annotations

import multiprocessing as mp
import os
import queue as thread_queue
import threading
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from control.state_machine import ErrorCode
from core.ipc import consume
from streaming.streaming_process import (
    StreamingProcess,
    build_pipeline_string,
)


def _lightweight_queues():
    def Q():
        return thread_queue.Queue(maxsize=20)
    return SimpleNamespace(
        stream_request=Q(), stream_status=Q(),
    )


# ════════════════════════════════════════════════════════════════
# build_pipeline_string — pure-function tests
# ════════════════════════════════════════════════════════════════
def test_pipeline_starts_with_v4l2src():
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
    )
    assert argv[0] == "gst-launch-1.0"
    assert argv[1] == "-e"
    pipeline = " ".join(argv)
    assert "v4l2src device=/dev/video0" in pipeline
    assert "format=NV12" in pipeline
    assert "framerate=30/1" in pipeline


def test_pipeline_has_tee_for_fanout():
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=True,
        bps=4_000_000, gop=30,
    )
    pipeline = " ".join(argv)
    # Must have tee + 3 branches (ai, display, stream)
    assert "tee name=t" in pipeline
    assert pipeline.count(" t. ") >= 3


def test_pipeline_skips_display_when_disabled():
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
    )
    pipeline = " ".join(argv)
    assert "kmssink" not in pipeline
    # Still has 2 branches (ai, stream)
    assert pipeline.count(" t. ") >= 2


def test_pipeline_udp_branch_uses_udpsink():
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
        host="192.168.42.100", port=5000,
    )
    pipeline = " ".join(argv)
    assert "rtph265pay" in pipeline
    assert "udpsink host=192.168.42.100 port=5000" in pipeline
    assert "srtsink" not in pipeline


def test_pipeline_srt_listener_branch():
    argv = build_pipeline_string(
        transport="srt_listener", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
        bind_addr="0.0.0.0", port=5000,
        srt_latency_ms=120, srt_streamid="patrol-ch2",
    )
    pipeline = " ".join(argv)
    assert "mpegtsmux" in pipeline
    assert "srtsink" in pipeline
    assert "mode=listener" in pipeline
    assert "latency=120" in pipeline
    assert "streamid=patrol-ch2" in pipeline


def test_pipeline_srt_caller_branch():
    argv = build_pipeline_string(
        transport="srt_caller", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
        host="10.0.0.5", port=5000,
        srt_streamid="patrol-ch2",
    )
    pipeline = " ".join(argv)
    assert "mode=caller" in pipeline
    assert "10.0.0.5:5000" in pipeline


def test_pipeline_unknown_transport_raises():
    with pytest.raises(ValueError, match="unknown transport"):
        build_pipeline_string(
            transport="websocket", device="/dev/video0",
            src_w=1920, src_h=1080, framerate=30,
            ai_shm_path="/tmp/ai.sock", display_enable=False,
            bps=4_000_000, gop=30,
        )


def test_encoder_bitrate_and_gop_propagate():
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=8_000_000, gop=15,
    )
    pipeline = " ".join(argv)
    assert "bps=8000000" in pipeline
    assert "gop=15" in pipeline
    assert "rc-mode=cbr" in pipeline


def test_ai_branch_uses_640x640_for_yolov5():
    """The AI branch must scale to YOLOv5's 640×640 input."""
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
    )
    pipeline = " ".join(argv)
    assert "videoscale" in pipeline
    assert "width=640,height=640" in pipeline
    assert "shmsink" in pipeline
    assert "/tmp/ai.sock" in pipeline


def test_stream_queue_is_leaky_for_backpressure():
    """The stream branch must be `leaky=downstream` so a slow network can't
    block the AI branch (matches AIRYS T_STREAM behavior)."""
    argv = build_pipeline_string(
        transport="udp", device="/dev/video0",
        src_w=1920, src_h=1080, framerate=30,
        ai_shm_path="/tmp/ai.sock", display_enable=False,
        bps=4_000_000, gop=30,
    )
    pipeline = " ".join(argv)
    # At least the stream-branch queue must be leaky
    assert "leaky=downstream" in pipeline


# ════════════════════════════════════════════════════════════════
# StreamingProcess lifecycle (using /bin/sh stub instead of gst-launch)
# ════════════════════════════════════════════════════════════════
def _make_proc(cfg_overrides=None):
    cfg = MagicMock()
    overrides = cfg_overrides or {}
    overrides.setdefault(("system", "cpu_affinity"), None)

    def _get(*keys, default=None):
        return overrides.get(tuple(keys), default)
    cfg.get.side_effect = _get

    proc = StreamingProcess.__new__(StreamingProcess)
    proc.queues = _lightweight_queues()
    proc.shutdown_event = mp.Event()
    proc.cfg = cfg
    proc.log = MagicMock()
    proc._proc = None
    proc._proc_lock = threading.Lock()
    proc._started_at = 0.0
    proc._stderr_thread = None
    proc._last_error = None
    proc._stats = {"starts": 0, "stops": 0, "crashes": 0, "stderr_lines": 0}
    proc._threads = []
    proc.spawn_thread = lambda target, name: threading.Thread(
        target=target, name=name, daemon=True).start() or None
    proc.is_running = lambda: not proc.shutdown_event.is_set()
    return proc


def _cleanup_proc(proc):
    """Set shutdown event so spawned threads exit."""
    proc.shutdown_event.set()


def test_start_invokes_subprocess_with_argv():
    proc = _make_proc()
    captured_argv = []

    class FakePopen:
        def __init__(self, argv, **kw):
            captured_argv.extend(argv)
            self.stderr = open(os.devnull, "rb")
        def poll(self): return None
        def send_signal(self, sig): pass
        def wait(self, timeout=None): return 0
        def kill(self): pass

    try:
        with patch("subprocess.Popen", FakePopen):
            proc._start_internal(creds={"ip": "192.168.42.100",
                                          "video_port": 5000})

        assert captured_argv[0] == "gst-launch-1.0"
        pipeline = " ".join(captured_argv)
        # Default transport is srt_listener → SRT bind addr (192.168.42.1) and port 5000
        assert "srtsink" in pipeline
        assert "192.168.42.1:5000" in pipeline
    finally:
        _cleanup_proc(proc)


def test_start_with_udp_transport_uses_creds_host():
    """When transport is UDP, the streamer dials creds['ip']:creds['video_port']."""
    proc = _make_proc(cfg_overrides={
        ("wifi", "stream_transport"): "udp",
    })
    captured_argv = []

    class FakePopen:
        def __init__(self, argv, **kw):
            captured_argv.extend(argv)
            self.stderr = open(os.devnull, "rb")
        def poll(self): return None
        def send_signal(self, sig): pass
        def wait(self, timeout=None): return 0
        def kill(self): pass

    try:
        with patch("subprocess.Popen", FakePopen):
            proc._start_internal(creds={"ip": "192.168.42.100",
                                          "video_port": 6000})

        pipeline = " ".join(captured_argv)
        assert "udpsink" in pipeline
        assert "host=192.168.42.100 port=6000" in pipeline
    finally:
        _cleanup_proc(proc)


def test_start_handles_gst_launch_not_installed():
    proc = _make_proc()
    try:
        with patch("subprocess.Popen", side_effect=FileNotFoundError("no gst-launch")):
            ok = proc._start_internal(creds={})
        assert ok is False
        proc.log.error.assert_called()
        # Failure status pushed
        msg = consume(proc.queues.stream_status, timeout=0.5)
        assert msg is not None
        assert msg["playing"] is False
    finally:
        _cleanup_proc(proc)


def test_stop_when_not_running_is_safe():
    proc = _make_proc()
    try:
        proc._stop_internal("test")     # no exception
        assert proc._proc is None
    finally:
        _cleanup_proc(proc)


def test_stderr_pattern_classifies_srt_handshake_error():
    """The stderr tail must surface SRT handshake errors as a typed code."""
    proc = _make_proc()
    try:
        # Synthetic stderr stream
        import io
        fake = io.BytesIO(b"some line\nERROR: SRT handshake failed: timeout\nmore\n")

        class FakeProc:
            stderr = fake
        proc._stderr_tail(FakeProc())
        assert proc._last_error == ErrorCode.SRT_HANDSHAKE
    finally:
        _cleanup_proc(proc)
