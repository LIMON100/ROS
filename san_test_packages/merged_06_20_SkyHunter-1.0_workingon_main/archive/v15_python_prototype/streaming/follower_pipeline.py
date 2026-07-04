"""Follower-side video send pipeline builder (PHASE 5 §5.1).

Companion to `streaming.gstreamer_relay` — the Hub's relay expects
follower streams to arrive as MPEG-TS over UDP+RTP (rtpmp2tpay), so the
follower's pipeline must end with `mpegtsmux ! rtpmp2tpay ! udpsink`.
This module is the pure-function pipeline-string builder, kept
distinct from the streaming subprocess manager so callers can validate
the argv without spawning gst-launch.

Quality presets (resolution × framerate × encoder bitrate) live here so
the leader-side `start_stream` command + the follower's local encoder
always agree on what the tablet asked for.

Pipeline shape:
    v4l2src
    → video/x-raw,W,H,fps
    → videoconvert
    → x265enc (or x264enc) tune=zerolatency speed-preset=ultrafast
    → h265parse (or h264parse)
    → mpegtsmux
    → rtpmp2tpay
    → udpsink host=<hub_ip> port=<udp_port>
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List

from core.messages import (
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_QUALITY_LOW,
    VIDEO_QUALITY_THUMBNAIL,
)


@dataclass(frozen=True)
class QualityPreset:
    """Resolution + framerate + target bitrate per quality label.

    `width`/`height` are the encoder input geometry; bitrate is the
    target the x264/x265 encoder is configured for. `framerate` is fps.
    """
    width: int
    height: int
    framerate: int
    bitrate_kbps: int


# Quality presets — kept consistent with
# streaming.gstreamer_relay.BITRATE_KBPS_BY_QUALITY so the bitrate the
# tablet sees in VideoResponse matches what the encoder actually does.
# The HD/FHD swap vs the spec text ("hd": 1920x1080) is deliberate —
# `VIDEO_QUALITY_FHD` is "Full HD" 1920x1080 and `VIDEO_QUALITY_HD` is
# 1280x720 per the v1.0 IDS labels; we match those, not the spec
# table's name swap.
QUALITY_PRESETS: Dict[str, QualityPreset] = {
    VIDEO_QUALITY_FHD:       QualityPreset(1920, 1080, 30, 8_000),
    VIDEO_QUALITY_HD:        QualityPreset(1280,  720, 30, 4_000),
    VIDEO_QUALITY_LOW:       QualityPreset( 640,  480, 15, 1_500),
    VIDEO_QUALITY_THUMBNAIL: QualityPreset( 320,  240,  5,   500),
}


# Encoder + parser names by codec. Both are software encoders here; the
# RK3588 BSP swaps in mpph265enc/mpph264enc via a config-driven override
# in the production process — that path lives in streaming_process.py.
_ENCODER_BY_CODEC: Dict[str, str] = {
    VIDEO_CODEC_H265: "x265enc",
    VIDEO_CODEC_H264: "x264enc",
}
_PARSER_BY_CODEC: Dict[str, str] = {
    VIDEO_CODEC_H265: "h265parse",
    VIDEO_CODEC_H264: "h264parse",
}


def build_follower_send_pipeline_argv(
    quality: str,
    codec: str,
    hub_ip: str,
    udp_port: int,
    device: str = "/dev/video0",
) -> List[str]:
    """argv for `gst-launch-1.0 -e <pipeline>`. Pure function.

    The pipeline tail (`mpegtsmux ! rtpmp2tpay ! udpsink`) is what the
    Hub-side relay (`udpsrc ! rtpmp2tdepay ! tsparse ! srtsink`)
    expects. Changing one without the other will yield a silent black
    feed on the tablet — the encoder is happy, the TS-over-RTP is
    happy, but tsparse won't find what it needs.
    """
    if quality not in QUALITY_PRESETS:
        raise ValueError(
            f"unknown quality: {quality!r}; expected one of "
            f"{sorted(QUALITY_PRESETS)}")
    if codec not in _ENCODER_BY_CODEC:
        raise ValueError(
            f"unknown codec: {codec!r}; expected one of "
            f"{sorted(_ENCODER_BY_CODEC)}")
    if not hub_ip:
        raise ValueError("hub_ip must be non-empty")
    if not 1 <= int(udp_port) <= 65535:
        raise ValueError(f"udp_port out of range: {udp_port}")

    preset = QUALITY_PRESETS[quality]
    encoder = _ENCODER_BY_CODEC[codec]
    parser  = _PARSER_BY_CODEC[codec]
    parts = [
        f"v4l2src device={device}",
        f"! video/x-raw,width={preset.width},height={preset.height},"
        f"framerate={preset.framerate}/1",
        "! videoconvert",
        f"! {encoder} bitrate={preset.bitrate_kbps} tune=zerolatency "
        f"speed-preset=ultrafast",
        f"! {parser}",
        "! mpegtsmux",
        "! rtpmp2tpay",
        f"! udpsink host={hub_ip} port={int(udp_port)} sync=false",
    ]
    pipeline = " ".join(parts)
    return ["gst-launch-1.0", "-e", "-q", *pipeline.split(" ")]


def preset_for(quality: str) -> QualityPreset:
    """Return the preset record for a quality label. Raises on unknown."""
    if quality not in QUALITY_PRESETS:
        raise ValueError(
            f"unknown quality: {quality!r}; expected one of "
            f"{sorted(QUALITY_PRESETS)}")
    return QUALITY_PRESETS[quality]


__all__ = (
    "QUALITY_PRESETS",
    "QualityPreset",
    "build_follower_send_pipeline_argv",
    "preset_for",
)
