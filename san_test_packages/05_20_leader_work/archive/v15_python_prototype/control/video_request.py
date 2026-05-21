"""Tablet → robot video request helpers.

Topic: /tablet/cmd/video_request  (QoS P1 RELIABLE)

The Android app sends a typed VideoRequest over WS JSON-RPC; the
``video.request`` handler in `WsTelemetryProcess._h_video_request`
parses the params into a `VideoRequest` via `parse_video_request` and
publishes on `queues.tablet_video_request`. The OrchestratorProcess
consumer filters by this robot's target_robot_id and translates into
the legacy stream_request dict format that StreamingProcess already
understands.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from core.messages import (
    VIDEO_ACTION_CHANGE_QUALITY,
    VIDEO_ACTION_START,
    VIDEO_ACTION_STOP,
    VIDEO_STATUS_ERROR,
    VIDEO_STATUS_STOPPED,
    VIDEO_STATUS_STREAMING,
    VideoRequest,
    VideoResponse,
)


def parse_video_request(params: Dict[str, Any]) -> VideoRequest:
    """Build a VideoRequest from a JSON-RPC `params` dict.

    Missing keys fall back to the dataclass defaults. Calls validate()
    before returning so the caller never sees an internally inconsistent
    message.
    """
    msg = VideoRequest(
        sequence=int(params.get("sequence", 0)),
        target_robot_id=int(params.get("target_robot_id", 0)),
        protocol=str(params.get("protocol", "srt")),
        desired_port=int(params.get("desired_port", 0)),
        codec=str(params.get("codec", "h265")),
        quality=str(params.get("quality", "hd")),
        encryption=bool(params.get("encryption", False)),
        passphrase_hint=str(params.get("passphrase_hint", "")),
        action=str(params.get("action", "start")),
        timestamp_ms=int(params.get("timestamp_ms", 0)),
    )
    msg.validate()
    return msg


def filter_for_robot(
    msg: VideoRequest, my_robot_id: int,
) -> Optional[VideoRequest]:
    """Return msg if it targets this robot, else None."""
    return msg if msg.target_robot_id == my_robot_id else None


def to_stream_requests(msg: VideoRequest) -> List[Dict[str, Any]]:
    """Translate a VideoRequest into one or two stream_request dicts.

    StreamingProcess only understands {"action": "start"|"stop", ...};
    `change_quality` is expressed as a stop→start pair so the GStreamer
    pipeline is rebuilt at the new bitrate. start/stop produce a single
    dict each. The list ordering matters — the consumer must publish in
    order.
    """
    if msg.action == VIDEO_ACTION_STOP:
        return [{"action": "stop"}]
    creds: Dict[str, Any] = {
        "video_port":      msg.desired_port,
        "protocol":        msg.protocol,
        "codec":           msg.codec,
        "quality":         msg.quality,
        "encryption":      msg.encryption,
        "passphrase_hint": msg.passphrase_hint,
    }
    start = {"action": "start", "creds": creds}
    if msg.action == VIDEO_ACTION_START:
        return [start]
    if msg.action == VIDEO_ACTION_CHANGE_QUALITY:
        return [{"action": "stop"}, start]
    return []


def build_video_response(
    last_request: Optional[VideoRequest],
    stream_status: Dict[str, Any],
    *,
    robot_id: int,
    stream_start_ms: int,
    passphrase: str,
    now_ms: int,
) -> VideoResponse:
    """Build one VideoResponse from a stream_status update.

    `last_request` carries the negotiated params (protocol, port, codec,
    quality, sequence) that the tablet originally asked for; pass None
    when the robot has no record (e.g. unsolicited stop on boot) and
    the defaults are used. `stream_status` is the raw dict published by
    StreamingProcess — keys read: `playing` (bool), `error_code` (int),
    `error_msg` (str, optional), `actual_bitrate_kbps` (int, optional).

    Status derivation:
      • error_code present and non-zero → "error"
      • playing == True                  → "streaming"
      • otherwise                        → "stopped"
    """
    err_code = stream_status.get("error_code") or 0
    playing  = bool(stream_status.get("playing", False))
    if err_code:
        status = VIDEO_STATUS_ERROR
    elif playing:
        status = VIDEO_STATUS_STREAMING
    else:
        status = VIDEO_STATUS_STOPPED

    if last_request is not None:
        sequence = last_request.sequence
        protocol = last_request.protocol
        port     = last_request.desired_port
        codec    = last_request.codec
        quality  = last_request.quality
    else:
        sequence = 0
        protocol = "srt"
        port     = 0
        codec    = "h265"
        quality  = "hd"

    error_msg = str(stream_status.get("error_msg", "")) if err_code else ""

    msg = VideoResponse(
        sequence=sequence,
        robot_id=robot_id,
        protocol=protocol,
        port=port,
        codec=codec,
        quality=quality,
        actual_bitrate_kbps=int(stream_status.get("actual_bitrate_kbps", 0)),
        passphrase=passphrase if status == VIDEO_STATUS_STREAMING else "",
        status=status,
        error_msg=error_msg,
        stream_start_ms=stream_start_ms if status == VIDEO_STATUS_STREAMING else 0,
        timestamp_ms=now_ms,
    )
    msg.validate()
    return msg


__all__ = (
    "build_video_response",
    "filter_for_robot",
    "parse_video_request",
    "to_stream_requests",
)
