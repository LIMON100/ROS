"""
StreamingProcess — manages the GStreamer UDP/SRT pipeline lifecycle.

Strategy: spawn `gst-launch-1.0` as a subprocess with a pipeline string
matching AIRYS SAN-STREAM-SW-001 Rev.D. We don't link against
GStreamer's C API from Python (PyGObject would be the alternative) —
running it as a subprocess keeps process boundaries clean (a GStreamer
crash kills the subprocess but not the robot's main control plane), and
the encode/transport runs on RK3588 hardware (mpph265enc) regardless of
how we drive it.

Three pipeline templates (chosen by config `wifi.stream_transport`):
  • UDP/RTP        : low-latency LAN streaming (no recovery)
  • SRT listener   : Wi-Fi link with 120 ms ARQ recovery (default)
  • SRT caller     : robot dials into a server-listener (rare)

The pipeline uses a `tee` element to fan the camera out to:
  1. AI consumer   — appsink-like NV12 pull (consumed by perception)
     [Phase B note: in our current Python pipeline, the AI side already
     gets frames via UnitreeGo2Adapter; the `tee` AI branch lands in
     /dev/shm so PerceptionProcess can mmap.]
  2. Stream branch — mpph265enc → rtph265pay → udpsink/srtsink
  3. Display       — kmssink (dev only; production headless)

Health monitoring: gst-launch's stderr is parsed for ERROR/WARN; if the
process exits unexpectedly, we publish stream_status(playing=False,
error_code=...) so the orchestrator can transition the FSM.
"""
from __future__ import annotations

import re
import signal
import subprocess
import threading
import time
from typing import List, Optional

from control.state_machine import ErrorCode
from core.base_process import BaseProcess
from core.ipc import consume, publish

# Command templates — keep these compact and use string formatting.
# Production-grade pipelines (with `queue` element tuning, leaky=2 etc.)
# are documented in include/streaming/gst_streamer.h.
#
# Common preamble (capture + tee fan-out):
#   v4l2src device={dev} ! video/x-raw,format=NV12,width=1920,height=1080,framerate=30/1
#   ! tee name=t
#
# AI branch:    t. ! queue ! videoscale ! video/x-raw,width=640,height=640
#                  ! shmsink socket-path=/tmp/airys_ai.sock wait-for-connection=false
# Display:      t. ! queue ! kmssink   (skipped when display.enable=false)
# Stream UDP:   t. ! queue leaky=2 ! mpph265enc bps={bps} gop={gop}
#                  ! h265parse ! rtph265pay ! udpsink host={host} port={port}
# Stream SRT:   t. ! queue leaky=2 ! mpph265enc bps={bps} gop={gop}
#                  ! h265parse ! mpegtsmux
#                  ! srtsink uri=srt://0.0.0.0:{port}?mode=listener


def build_pipeline_string(*, transport: str, device: str,
                           src_w: int, src_h: int, framerate: int,
                           ai_shm_path: str, display_enable: bool,
                           bps: int, gop: int,
                           # UDP
                           host: str = "192.168.42.100", port: int = 5000,
                           # SRT
                           bind_addr: str = "0.0.0.0",
                           srt_latency_ms: int = 120,
                           srt_streamid: str = "patrol-ch2",
                           ) -> List[str]:
    """Return argv for `gst-launch-1.0 -e <pipeline>`.

    Kept as a pure function so the test suite can validate the pipeline
    text without spawning gst-launch.
    """
    # Source + tee
    parts = [
        f"v4l2src device={device}",
        f"! video/x-raw,format=NV12,width={src_w},height={src_h},framerate={framerate}/1",
        "! tee name=t",
    ]

    # AI branch — shmsink for zero-copy IPC into PerceptionProcess
    parts += [
        "t.", "! queue max-size-buffers=2 leaky=downstream",
        "! videoscale ! video/x-raw,width=640,height=640",
        f"! shmsink socket-path={ai_shm_path} wait-for-connection=false sync=false",
    ]

    # Display branch (dev only)
    if display_enable:
        parts += [
            "t.", "! queue max-size-buffers=2 leaky=downstream",
            "! videoconvert ! kmssink sync=false",
        ]

    # Stream branch
    parts += [
        "t.", "! queue max-size-buffers=3 leaky=downstream",
        f"! mpph265enc bps={bps} gop={gop} rc-mode=cbr",
        "! h265parse",
    ]
    if transport == "udp":
        parts += [
            "! rtph265pay pt=96 config-interval=1",
            f"! udpsink host={host} port={port} sync=false",
        ]
    elif transport == "srt_listener":
        srt_uri = (f"srt://{bind_addr}:{port}?mode=listener"
                    f"&latency={srt_latency_ms}"
                    f"&streamid={srt_streamid}")
        parts += [
            "! mpegtsmux",
            f"! srtsink uri={srt_uri} sync=false",
        ]
    elif transport == "srt_caller":
        srt_uri = (f"srt://{host}:{port}?mode=caller"
                    f"&latency={srt_latency_ms}"
                    f"&streamid={srt_streamid}")
        parts += [
            "! mpegtsmux",
            f"! srtsink uri={srt_uri} sync=false",
        ]
    else:
        raise ValueError(f"unknown transport: {transport}")

    pipeline = " ".join(parts)
    return ["gst-launch-1.0", "-e", "-q", *pipeline.split(" ")]


class StreamingProcess(BaseProcess):
    """Subprocess-based GStreamer manager.

    Listens on `stream_request` for {"action":"start"|"stop", ...} and
    starts/stops gst-launch accordingly. Publishes `stream_status` with
    playing/dropped/bitrate fields the orchestrator needs.
    """

    PLAYING_PROBE_S = 1.5    # how long to wait before declaring "playing"

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="Streaming", shutdown_event=shutdown_event,
            rate_hz=1.0,
            cpu_affinity=config.get("system", "cpu_affinity", "stream"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._proc: Optional[subprocess.Popen] = None
        self._proc_lock = threading.Lock()
        self._started_at: float = 0.0
        self._stderr_thread: Optional[threading.Thread] = None
        self._last_error: Optional[ErrorCode] = None
        self._stats = {"starts": 0, "stops": 0, "crashes": 0,
                        "stderr_lines": 0}

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        self.spawn_thread(self._request_loop, name="StreamReq")

    def step(self) -> None:
        # Health check: if subprocess has died, push a stream_status(False)
        with self._proc_lock:
            proc = self._proc
        if proc is None:
            return
        rc = proc.poll()
        if rc is not None:
            self.log.error(f"gst-launch died (rc={rc})")
            self._stats["crashes"] += 1
            err = self._last_error or ErrorCode.STREAM_FAIL
            with self._proc_lock:
                self._proc = None
            publish(self.queues.stream_status, {
                "playing": False, "error_code": int(err),
                "exit_code": rc,
            })
        elif (time.monotonic() - self._started_at) > self.PLAYING_PROBE_S:
            # Still alive after probe window — we're playing
            publish(self.queues.stream_status, {
                "playing": True,
                "uptime_s": time.monotonic() - self._started_at,
            })

    def teardown(self) -> None:
        self._stop_internal("teardown")

    # ───────── Request loop ─────────
    def _request_loop(self):
        while self.is_running():
            req = consume(self.queues.stream_request, timeout=0.2)
            if req is None:
                continue
            action = req.get("action")
            if action == "start":
                self._start_internal(req.get("creds") or {})
            elif action == "stop":
                self._stop_internal("requested")
            else:
                self.log.warning(f"unknown stream request: {req}")

    # ───────── Start / Stop ─────────
    def _start_internal(self, creds: dict) -> bool:
        with self._proc_lock:
            if self._proc is not None and self._proc.poll() is None:
                self.log.info("stream already running, ignoring start")
                return True

            argv = self._build_argv(creds)
            self.log.info(f"starting GStreamer: {' '.join(argv[:5])}…")

            try:
                self._proc = subprocess.Popen(
                    argv,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    text=False,
                )
            except FileNotFoundError:
                self.log.error("gst-launch-1.0 not found; install gstreamer1.0")
                publish(self.queues.stream_status, {
                    "playing": False,
                    "error_code": int(ErrorCode.STREAM_FAIL),
                })
                return False
            except OSError as e:
                self.log.error(f"failed to spawn gst-launch: {e}")
                publish(self.queues.stream_status, {
                    "playing": False,
                    "error_code": int(ErrorCode.STREAM_FAIL),
                })
                return False

            self._started_at = time.monotonic()
            self._last_error = None
            self._stats["starts"] += 1

            # Stderr tail thread — watches for SRT/encoder errors
            self._stderr_thread = threading.Thread(
                target=self._stderr_tail, args=(self._proc,),
                name="StreamStderr", daemon=True,
            )
            self._stderr_thread.start()
            return True

    def _stop_internal(self, reason: str) -> None:
        with self._proc_lock:
            if self._proc is None:
                return
            self.log.info(f"stopping GStreamer ({reason})")
            try:
                # GStreamer needs SIGINT for graceful EOS handling (the -e flag).
                self._proc.send_signal(signal.SIGINT)
                self._proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait(timeout=1.0)
            except OSError:
                pass
            self._proc = None
            self._stats["stops"] += 1
        publish(self.queues.stream_status, {"playing": False, "stopped_by": reason})

    def _build_argv(self, creds: dict) -> List[str]:
        return build_pipeline_string(
            transport=self.cfg.get("wifi", "stream_transport",
                                    default="srt_listener"),
            device=self.cfg.get("stream", "v4l2_device",
                                 default="/dev/video0"),
            src_w=int(self.cfg.get("stream", "src_w",  default=1920)),
            src_h=int(self.cfg.get("stream", "src_h",  default=1080)),
            framerate=int(self.cfg.get("stream", "framerate", default=30)),
            ai_shm_path=self.cfg.get("stream", "ai_shm_path",
                                      default="/tmp/patrol_ai.sock"),
            display_enable=bool(self.cfg.get("stream", "display_enable",
                                              default=False)),
            bps=int(self.cfg.get("stream", "bitrate_bps", default=4_000_000)),
            gop=int(self.cfg.get("stream", "gop", default=30)),
            host=creds.get("ip", "192.168.42.100"),
            port=int(creds.get("video_port", 5000)),
            bind_addr=self.cfg.get("wifi", "ap_ip", default="192.168.42.1"),
            srt_latency_ms=int(self.cfg.get("stream", "srt_latency_ms",
                                              default=120)),
            srt_streamid=self.cfg.get("stream", "srt_streamid",
                                        default="patrol-ch2"),
        )

    # ───────── Stderr parsing ─────────
    _ERR_PATTERNS = [
        (re.compile(rb"srt.*hand[s]?hake.*fail", re.I),
         ErrorCode.SRT_HANDSHAKE),
        (re.compile(rb"could not.*open|no such device", re.I),
         ErrorCode.BAD_INTERFACE),
        (re.compile(rb"ERROR.*v4l2|v4l2.*error", re.I),
         ErrorCode.STREAM_FAIL),
    ]

    def _stderr_tail(self, proc: subprocess.Popen):
        if proc.stderr is None:
            return
        for raw in proc.stderr:
            self._stats["stderr_lines"] += 1
            for pat, err in self._ERR_PATTERNS:
                if pat.search(raw):
                    self._last_error = err
                    self.log.error(
                        f"gst stderr matched {err.name}: "
                        f"{raw.decode(errors='replace').strip()[:120]}")
                    break
