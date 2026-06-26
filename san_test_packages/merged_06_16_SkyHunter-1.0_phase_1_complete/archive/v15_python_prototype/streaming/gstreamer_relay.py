"""Hub UGV SBC#2 GStreamer relay — follower UDP → tablet SRT.

PHASE 4 of the v1.0→v1.1 IDS rollout (SAN-SDD-SUR-001 v1.1 §5).

Pull-style flow:
    Tablet → VideoRequest → Hub
                            ↓
                            start_stream cmd → Follower
                            ↓
                            Follower streams UDP/RTP to Hub
                            ↓
                            Hub GStreamer: udpsrc → ts depay → srtsink
                            ↓
                            VideoResponse(srt_uri=...) → Tablet

This module owns the Hub-side local pipeline lifecycle plus the helpers
needed to build the response. It does NOT publish to any queue and does
NOT send the follower start command — those are wired up by a higher-
level orchestrator (a Hub-side `comm` process, follow-up). Subprocess
spawning is injectable so the test suite can validate the state
machine without invoking `gst-launch-1.0`.

Pipeline contract:
  • Follower wraps its H.265/H.264 elementary stream in MPEG-TS and
    sends it over UDP with rtpmp2tpay. That matches the PHASE 4 spec
    pipeline (`udpsrc → rtpmp2tdepay → tsparse → srtsink`). A follower
    that still uses raw H.265-over-RTP (rtph265pay) is not compatible
    with this relay — convert it to mpegtsmux+rtpmp2tpay first.
"""
from __future__ import annotations

import secrets
import subprocess
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Optional

from control.video_request import build_video_response
from core.messages import (
    VIDEO_ACTION_CHANGE_QUALITY,
    VIDEO_ACTION_START,
    VIDEO_ACTION_STOP,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_QUALITY_LOW,
    VIDEO_QUALITY_THUMBNAIL,
    VideoRequest,
    VideoResponse,
)

# Port allocation — leader/Hub assigns one UDP port per follower for the
# inbound leg, one SRT port per follower for the outbound (tablet) leg.
# Stays clear of the 5000 used by StreamingProcess defaults so a single
# host can run both follower and Hub roles in sim.
BASE_UDP_PORT_FOLLOWER_TO_HUB = 5000
BASE_SRT_PORT_HUB_TO_TABLET   = 8888

# 128-bit passphrase encoded as 32 hex chars; SRT accepts 10–79 chars
# (libsrt internally PBKDF2-derives the AES key). 32 hex is well within.
PASSPHRASE_BYTES = 16

# Default SRT receive latency. 120 ms is the LAN/Wi-Fi default; bump to
# 200–300 ms when the tablet is on LTE.
DEFAULT_SRT_LATENCY_MS = 120

# Target encoder bitrate per quality preset. Matches the follower-side
# StreamingProcess defaults — the relay echoes this back to the tablet
# in VideoResponse.actual_bitrate_kbps so the UI can show "you asked
# for HD, you're getting X kbps."
BITRATE_KBPS_BY_QUALITY: Dict[str, int] = {
    VIDEO_QUALITY_THUMBNAIL:    500,
    VIDEO_QUALITY_LOW:        1_500,
    VIDEO_QUALITY_HD:         4_000,
    VIDEO_QUALITY_FHD:        8_000,
}

# Maximum concurrent FHD/HD pipelines before the relay auto-downgrades
# new requests to thumbnail to protect the Wi-Fi 6 mesh budget. The cap
# is 3 because at FHD 8 Mbps × 3 = 24 Mbps — already over half the
# usable mesh capacity once SLAM + telemetry are accounted for.
MAX_CONCURRENT_FHD = 3

# Qualities considered "heavy" for the concurrency cap. The cap counts
# pipelines at these qualities; low/thumbnail don't count and are never
# downgraded.
_HEAVY_QUALITIES = (VIDEO_QUALITY_HD, VIDEO_QUALITY_FHD)


PopenFactory = Callable[[List[str]], "subprocess.Popen"]


# ─── port + URI helpers ────────────────────────────────────────────────

def udp_port_for(robot_id: int) -> int:
    """Inbound UDP port the Hub listens on for a follower's stream."""
    if robot_id < 0:
        raise ValueError(f"robot_id must be non-negative: {robot_id}")
    port = BASE_UDP_PORT_FOLLOWER_TO_HUB + int(robot_id)
    if port > 65535:
        raise ValueError(f"udp_port out of range for robot_id={robot_id}")
    return port


def srt_port_for(robot_id: int) -> int:
    """Outbound SRT port the Hub binds for the tablet to connect to."""
    if robot_id < 0:
        raise ValueError(f"robot_id must be non-negative: {robot_id}")
    port = BASE_SRT_PORT_HUB_TO_TABLET + int(robot_id)
    if port > 65535:
        raise ValueError(f"srt_port out of range for robot_id={robot_id}")
    return port


def generate_passphrase(token_hex: Callable[[int], str] = secrets.token_hex) -> str:
    """Per-stream passphrase. `token_hex` is injectable for deterministic tests."""
    return token_hex(PASSPHRASE_BYTES)


def build_srt_uri(
    host: str,
    port: int,
    mode: str,
    *,
    latency_ms: int = DEFAULT_SRT_LATENCY_MS,
    passphrase: Optional[str] = None,
    streamid: Optional[str] = None,
) -> str:
    """Build an SRT URI. `mode` is 'listener' or 'caller'.

    Hub's outbound (tablet-facing) side is mode=listener; the tablet
    dials in as mode=caller. The two SRT URIs differ only in mode and
    the host portion (tablet sees the Hub's external IP; the Hub binds
    `0.0.0.0`).
    """
    if mode not in ("listener", "caller"):
        raise ValueError(f"srt mode must be listener|caller: {mode!r}")
    qs = [f"mode={mode}", f"latency={int(latency_ms)}"]
    if passphrase:
        qs.append(f"passphrase={passphrase}")
    if streamid:
        qs.append(f"streamid={streamid}")
    return f"srt://{host}:{int(port)}?" + "&".join(qs)


def build_relay_pipeline_argv(
    udp_port: int,
    srt_uri: str,
    codec: str,
) -> List[str]:
    """argv for `gst-launch-1.0 -e <pipeline>` — pure function.

    Pipeline: udpsrc → rtpmp2tdepay → tsparse → srtsink.

    The follower must be encoding its stream as MPEG-TS over RTP
    (mpegtsmux + rtpmp2tpay). Codec is encoded into `streamid` rather
    than the pipeline itself — gstreamer's TS demuxer figures out
    h264/h265 from the stream's PMT.
    """
    if codec not in (VIDEO_CODEC_H264, VIDEO_CODEC_H265):
        raise ValueError(f"unknown codec: {codec!r}")
    parts = [
        f"udpsrc port={udp_port} caps=application/x-rtp",
        "! rtpmp2tdepay",
        "! tsparse",
        f"! srtsink uri={srt_uri} sync=false",
    ]
    pipeline = " ".join(parts)
    return ["gst-launch-1.0", "-e", "-q", *pipeline.split(" ")]


def build_follower_start_cmd(
    request: VideoRequest,
    udp_port: int,
    *,
    hub_ip: str,
) -> Dict[str, Any]:
    """Construct the `stream_request` dict the Hub should send to the
    follower, telling it where to push its UDP/RTP stream.

    The caller publishes this dict on whatever follower-bound queue
    exists (orchestrator-specific); the relay doesn't own that wire.
    """
    return {
        "action": "start",
        "creds": {
            "video_port": udp_port,
            "host":       hub_ip,
            "protocol":   "udp",     # follower → Hub leg is UDP/RTP/TS
            "codec":      request.codec,
            "quality":    request.quality,
            "encryption": False,     # encryption applies on the SRT leg
            "passphrase_hint": "",
        },
    }


# ─── relay state machine ───────────────────────────────────────────────

@dataclass
class _RelayHandle:
    """One running pipeline + its allocated ports + the AES passphrase."""
    robot_id: int
    udp_port: int
    srt_port: int
    passphrase: str
    proc: Any                       # subprocess.Popen or test double
    quality: str
    codec: str
    started_ms: int
    last_request: VideoRequest


class GStreamerRelay:
    """Hub-side pipeline manager.

    One pipeline per `target_robot_id`. Re-starting the same robot
    terminates the old pipeline and spawns a fresh one — that's the
    documented `change_quality` semantics and also covers the "tablet
    re-asks for an already-running stream" race.

    Threading: not internally synchronised. The integrator owns the
    lock; for the planned Hub `comm` process that's a single thread
    handling the tablet → relay path, so no lock is required there.
    """

    def __init__(
        self,
        hub_external_ip: str,
        *,
        popen_factory: Optional[PopenFactory] = None,
        token_hex: Callable[[int], str] = secrets.token_hex,
        srt_latency_ms: int = DEFAULT_SRT_LATENCY_MS,
    ):
        if not hub_external_ip:
            raise ValueError("hub_external_ip must be non-empty")
        self.hub_external_ip = hub_external_ip
        self._popen: PopenFactory = popen_factory or subprocess.Popen
        self._token_hex = token_hex
        self._srt_latency_ms = int(srt_latency_ms)
        self._pipelines: Dict[int, _RelayHandle] = {}

    # ─── introspection ─────────────────────────────────────────────────

    def is_active(self, robot_id: int) -> bool:
        return int(robot_id) in self._pipelines

    def handle_for(self, robot_id: int) -> Optional[_RelayHandle]:
        return self._pipelines.get(int(robot_id))

    @property
    def active_robots(self) -> List[int]:
        return sorted(self._pipelines)

    # ─── dispatch ──────────────────────────────────────────────────────

    def on_video_request(
        self,
        request: VideoRequest,
        *,
        now_ms: int,
    ) -> VideoResponse:
        """Apply a VideoRequest. Returns the VideoResponse to send back."""
        request.validate()
        if request.action == VIDEO_ACTION_START:
            return self.start(request, now_ms=now_ms)
        if request.action == VIDEO_ACTION_STOP:
            return self.stop(request.target_robot_id, now_ms=now_ms,
                              last_request=request)
        if request.action == VIDEO_ACTION_CHANGE_QUALITY:
            return self.change_quality(request, now_ms=now_ms)
        # VideoRequest.validate() rejects unknown actions, so this is
        # genuinely unreachable — keep the explicit raise for safety.
        raise ValueError(f"unknown action: {request.action!r}")

    # ─── lifecycle ─────────────────────────────────────────────────────

    def start(self, request: VideoRequest, *, now_ms: int) -> VideoResponse:
        """Spawn a relay pipeline for the targeted robot.

        If a pipeline is already running for this robot, it is replaced
        (the old subprocess is terminated). That handles a tablet's
        retry-on-timeout, a stale orphan from a prior session, and the
        change_quality stop+start path.

        When the swarm already has `MAX_CONCURRENT_FHD` heavy (HD/FHD)
        pipelines running and another HD/FHD request arrives, the
        incoming request is auto-downgraded to thumbnail. This protects
        the Wi-Fi 6 mesh budget — three concurrent FHD streams at
        8 Mbps already consume ~24 Mbps before SLAM + telemetry. The
        replaced-robot case bypasses the cap (replacing one's own
        slot doesn't add to concurrency).
        """
        rid = int(request.target_robot_id)
        replacing = rid in self._pipelines
        if replacing:
            self._terminate(rid)

        effective_quality = request.quality
        if (not replacing
                and request.quality in _HEAVY_QUALITIES
                and self._heavy_active_count() >= MAX_CONCURRENT_FHD):
            effective_quality = VIDEO_QUALITY_THUMBNAIL

        udp_port = udp_port_for(rid)
        srt_port = srt_port_for(rid)
        passphrase = generate_passphrase(self._token_hex)

        # Hub binds 0.0.0.0 (listener) — the tablet uses the external IP.
        bind_uri = build_srt_uri(
            host="0.0.0.0", port=srt_port, mode="listener",
            latency_ms=self._srt_latency_ms, passphrase=passphrase,
            streamid=f"robot{rid}-{request.codec}")
        argv = build_relay_pipeline_argv(
            udp_port=udp_port, srt_uri=bind_uri, codec=request.codec)
        proc = self._popen(argv)

        self._pipelines[rid] = _RelayHandle(
            robot_id=rid, udp_port=udp_port, srt_port=srt_port,
            passphrase=passphrase, proc=proc,
            quality=effective_quality, codec=request.codec,
            started_ms=now_ms, last_request=request,
        )
        return self._response_for(rid, status_playing=True, now_ms=now_ms)

    def _heavy_active_count(self) -> int:
        """Count concurrent HD/FHD pipelines for the downgrade gate."""
        return sum(
            1 for h in self._pipelines.values()
            if h.quality in _HEAVY_QUALITIES)

    def stop(
        self,
        robot_id: int,
        *,
        now_ms: int,
        last_request: Optional[VideoRequest] = None,
    ) -> VideoResponse:
        """Terminate the pipeline for a robot, if running."""
        rid = int(robot_id)
        if rid in self._pipelines:
            self._terminate(rid)
        return build_video_response(
            last_request=last_request,
            stream_status={"playing": False},
            robot_id=rid,
            stream_start_ms=0,
            passphrase="",
            now_ms=now_ms,
        )

    def change_quality(
        self,
        request: VideoRequest,
        *,
        now_ms: int,
    ) -> VideoResponse:
        """Apply a new quality preset by tearing down + re-spawning.

        We could in principle drive `bps` via `gst-launch`'s set-property
        runtime hook, but spawn cost on RK3588 is ~200 ms and the spec
        treats change_quality as a hard reset.
        """
        return self.start(request, now_ms=now_ms)

    def shutdown(self) -> None:
        """Tear down every running pipeline. Idempotent."""
        for rid in list(self._pipelines):
            self._terminate(rid)

    # ─── internals ─────────────────────────────────────────────────────

    def _terminate(self, robot_id: int) -> None:
        handle = self._pipelines.pop(robot_id, None)
        if handle is None:
            return
        # gst-launch handles SIGINT (clean EOS) when started with `-e`.
        try:
            handle.proc.terminate()
        except Exception:
            # The integrator's metrics will catch a non-terminating
            # subprocess; we don't want a stale entry in self._pipelines
            # to mask a future restart.
            pass

    def _response_for(
        self,
        robot_id: int,
        *,
        status_playing: bool,
        now_ms: int,
    ) -> VideoResponse:
        handle = self._pipelines[robot_id]
        bitrate = BITRATE_KBPS_BY_QUALITY.get(handle.quality, 0)
        # The tablet connects to the *external* IP as caller — note this
        # is not the same URI the Hub binds to (bind=0.0.0.0).
        public_uri = build_srt_uri(
            host=self.hub_external_ip, port=handle.srt_port,
            mode="caller", latency_ms=self._srt_latency_ms,
            passphrase=handle.passphrase,
            streamid=f"robot{robot_id}-{handle.codec}")
        msg = VideoResponse(
            sequence=handle.last_request.sequence,
            robot_id=robot_id,
            protocol=handle.last_request.protocol,
            port=handle.srt_port,
            srt_uri=public_uri,
            codec=handle.codec,
            quality=handle.quality,
            actual_bitrate_kbps=bitrate,
            passphrase=handle.passphrase if status_playing else "",
            status="streaming" if status_playing else "stopped",
            error_msg="",
            stream_start_ms=handle.started_ms if status_playing else 0,
            timestamp_ms=now_ms,
        )
        msg.validate()
        return msg


__all__ = (
    "BASE_SRT_PORT_HUB_TO_TABLET",
    "BASE_UDP_PORT_FOLLOWER_TO_HUB",
    "BITRATE_KBPS_BY_QUALITY",
    "DEFAULT_SRT_LATENCY_MS",
    "GStreamerRelay",
    "MAX_CONCURRENT_FHD",
    "PASSPHRASE_BYTES",
    "build_follower_start_cmd",
    "build_relay_pipeline_argv",
    "build_srt_uri",
    "generate_passphrase",
    "srt_port_for",
    "udp_port_for",
)
