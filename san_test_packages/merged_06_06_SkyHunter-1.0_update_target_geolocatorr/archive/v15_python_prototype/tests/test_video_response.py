"""Tests for VideoResponse + build_video_response (robot → tablet)."""
import pytest

from control.video_request import build_video_response
from core.messages import (
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_PROTOCOL_SRT,
    VIDEO_PROTOCOL_UDP,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_STATUS_ERROR,
    VIDEO_STATUS_STOPPED,
    VIDEO_STATUS_STREAMING,
    VideoRequest,
    VideoResponse,
)

# ─── VideoResponse dataclass validators ─────────────────────────────────

def test_validate_accepts_defaults():
    VideoResponse().validate()


def test_validate_rejects_unknown_status():
    with pytest.raises(ValueError):
        VideoResponse(status="restarting").validate()


def test_validate_rejects_unknown_protocol():
    with pytest.raises(ValueError):
        VideoResponse(protocol="rtmp").validate()


def test_validate_rejects_unknown_codec():
    with pytest.raises(ValueError):
        VideoResponse(codec="av1").validate()


def test_validate_rejects_unknown_quality():
    with pytest.raises(ValueError):
        VideoResponse(quality="ultra").validate()


@pytest.mark.parametrize("port", [-1, 65536])
def test_validate_rejects_port_out_of_range(port):
    with pytest.raises(ValueError):
        VideoResponse(port=port).validate()


def test_validate_rejects_negative_bitrate():
    with pytest.raises(ValueError):
        VideoResponse(actual_bitrate_kbps=-1).validate()


# ─── srt_uri (v1.1 VideoStreamHandle field) ─────────────────────────────

def test_validate_accepts_empty_srt_uri_back_compat():
    # v1.0 peers leave srt_uri blank; that path must still validate.
    VideoResponse(srt_uri="").validate()


def test_validate_accepts_srt_scheme():
    VideoResponse(
        srt_uri="srt://10.0.0.5:8888?mode=listener&streamid=robot7",
    ).validate()


def test_validate_accepts_udp_scheme():
    VideoResponse(srt_uri="udp://10.0.0.5:5000").validate()


def test_validate_rejects_bad_srt_uri_scheme():
    with pytest.raises(ValueError):
        VideoResponse(srt_uri="rtmp://10.0.0.5/live").validate()


def test_validate_rejects_garbled_srt_uri():
    with pytest.raises(ValueError):
        VideoResponse(srt_uri="not-a-uri").validate()


# ─── build_video_response — happy paths ────────────────────────────────

def _req(**overrides):
    base = dict(
        sequence=42, target_robot_id=3,
        protocol=VIDEO_PROTOCOL_UDP, desired_port=5050,
        codec=VIDEO_CODEC_H264, quality=VIDEO_QUALITY_FHD,
    )
    base.update(overrides)
    return VideoRequest(**base)


def test_build_streaming_status_when_playing():
    resp = build_video_response(
        _req(), {"playing": True, "actual_bitrate_kbps": 8000},
        robot_id=3, stream_start_ms=1_700_000_000_000,
        passphrase="secret-x", now_ms=1_700_000_000_500,
    )
    assert resp.status == VIDEO_STATUS_STREAMING
    assert resp.sequence == 42
    assert resp.robot_id == 3
    assert resp.protocol == VIDEO_PROTOCOL_UDP
    assert resp.port == 5050
    assert resp.codec == VIDEO_CODEC_H264
    assert resp.quality == VIDEO_QUALITY_FHD
    assert resp.actual_bitrate_kbps == 8000
    assert resp.passphrase == "secret-x"
    assert resp.stream_start_ms == 1_700_000_000_000
    assert resp.error_msg == ""


def test_build_stopped_status_when_not_playing():
    resp = build_video_response(
        _req(), {"playing": False},
        robot_id=3, stream_start_ms=0, passphrase="secret-x",
        now_ms=1_000,
    )
    assert resp.status == VIDEO_STATUS_STOPPED
    # passphrase + stream_start_ms cleared when not streaming
    assert resp.passphrase == ""
    assert resp.stream_start_ms == 0


def test_build_error_status_when_error_code_nonzero():
    resp = build_video_response(
        _req(), {"playing": False, "error_code": 7, "error_msg": "boom"},
        robot_id=3, stream_start_ms=0, passphrase="secret",
        now_ms=1_000,
    )
    assert resp.status == VIDEO_STATUS_ERROR
    assert resp.error_msg == "boom"
    # error always wins, even if playing somehow still True
    resp2 = build_video_response(
        _req(), {"playing": True, "error_code": 9},
        robot_id=3, stream_start_ms=1, passphrase="secret",
        now_ms=1_000,
    )
    assert resp2.status == VIDEO_STATUS_ERROR


def test_build_with_no_prior_request_uses_defaults():
    resp = build_video_response(
        None, {"playing": True},
        robot_id=3, stream_start_ms=2_000, passphrase="p",
        now_ms=3_000,
    )
    assert resp.sequence == 0
    assert resp.protocol == VIDEO_PROTOCOL_SRT     # default
    assert resp.codec == VIDEO_CODEC_H265          # default
    assert resp.quality == VIDEO_QUALITY_HD        # default
    assert resp.status == VIDEO_STATUS_STREAMING


def test_build_echoes_changed_quality_from_last_request():
    # Tablet asked for change_quality → low; the response should echo
    # whatever the last accepted request set, not the previous quality.
    req = _req(quality="low")
    resp = build_video_response(
        req, {"playing": True}, robot_id=3,
        stream_start_ms=1, passphrase="x", now_ms=2,
    )
    assert resp.quality == "low"


def test_build_zero_bitrate_when_status_missing_field():
    resp = build_video_response(
        _req(), {"playing": True},   # no actual_bitrate_kbps key
        robot_id=3, stream_start_ms=1, passphrase="x", now_ms=2,
    )
    assert resp.actual_bitrate_kbps == 0


def test_build_passes_validate():
    # build_video_response calls validate() internally — confirm with a
    # combination that exercises all enums.
    req = _req(protocol="srt", codec="h265", quality="thumbnail")
    resp = build_video_response(
        req, {"playing": True}, robot_id=1,
        stream_start_ms=1, passphrase="", now_ms=2,
    )
    resp.validate()  # explicit re-check; already called inside builder
