"""Tests for /tablet/cmd/video_request (VideoRequest + helpers)."""
import pytest

from control.video_request import (
    filter_for_robot,
    parse_video_request,
    to_stream_requests,
)
from core.messages import (
    VIDEO_ACTION_CHANGE_QUALITY,
    VIDEO_ACTION_START,
    VIDEO_ACTION_STOP,
    VIDEO_ACTIONS,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_CODECS,
    VIDEO_PROTOCOL_SRT,
    VIDEO_PROTOCOL_UDP,
    VIDEO_PROTOCOLS,
    VIDEO_QUALITIES,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_QUALITY_LOW,
    VIDEO_QUALITY_THUMBNAIL,
    VideoRequest,
)

# ─── VideoRequest dataclass ────────────────────────────────────────────

def test_validate_accepts_defaults():
    VideoRequest().validate()


def test_validate_rejects_unknown_protocol():
    with pytest.raises(ValueError):
        VideoRequest(protocol="rtmp").validate()


def test_validate_rejects_unknown_codec():
    with pytest.raises(ValueError):
        VideoRequest(codec="av1").validate()


def test_validate_rejects_unknown_quality():
    with pytest.raises(ValueError):
        VideoRequest(quality="ultra").validate()


def test_validate_rejects_unknown_action():
    with pytest.raises(ValueError):
        VideoRequest(action="pause").validate()


@pytest.mark.parametrize("port", [-1, 65536, 100000])
def test_validate_rejects_port_out_of_range(port):
    with pytest.raises(ValueError):
        VideoRequest(desired_port=port).validate()


def test_validate_accepts_port_zero_and_max():
    VideoRequest(desired_port=0).validate()
    VideoRequest(desired_port=65535).validate()


def test_all_enum_combinations_validate():
    for proto in VIDEO_PROTOCOLS:
        for codec in VIDEO_CODECS:
            for quality in VIDEO_QUALITIES:
                for action in VIDEO_ACTIONS:
                    VideoRequest(
                        protocol=proto, codec=codec,
                        quality=quality, action=action,
                    ).validate()


# ─── parse_video_request ───────────────────────────────────────────────

def test_parse_happy_path():
    params = {
        "sequence": 7, "target_robot_id": 3,
        "protocol": "udp", "desired_port": 5000,
        "codec": "h264", "quality": "fhd",
        "encryption": True, "passphrase_hint": "blue-fox",
        "action": "change_quality", "timestamp_ms": 1_700_000_000_000,
    }
    msg = parse_video_request(params)
    assert msg.sequence == 7
    assert msg.target_robot_id == 3
    assert msg.protocol == VIDEO_PROTOCOL_UDP
    assert msg.desired_port == 5000
    assert msg.codec == VIDEO_CODEC_H264
    assert msg.quality == VIDEO_QUALITY_FHD
    assert msg.encryption is True
    assert msg.passphrase_hint == "blue-fox"
    assert msg.action == VIDEO_ACTION_CHANGE_QUALITY


def test_parse_uses_defaults_for_missing_fields():
    msg = parse_video_request({"target_robot_id": 2})
    assert msg.protocol == VIDEO_PROTOCOL_SRT       # default
    assert msg.codec == VIDEO_CODEC_H265            # default
    assert msg.quality == VIDEO_QUALITY_HD          # default
    assert msg.action == VIDEO_ACTION_START         # default


def test_parse_rejects_bad_enum_via_validate():
    with pytest.raises(ValueError):
        parse_video_request({"protocol": "ftp"})


def test_parse_coerces_string_ints():
    msg = parse_video_request({"target_robot_id": "5", "desired_port": "5000"})
    assert msg.target_robot_id == 5
    assert msg.desired_port == 5000


# ─── filter_for_robot ───────────────────────────────────────────────────

def test_filter_matches_self():
    msg = VideoRequest(target_robot_id=4)
    assert filter_for_robot(msg, my_robot_id=4) is msg


def test_filter_drops_others():
    msg = VideoRequest(target_robot_id=4)
    assert filter_for_robot(msg, my_robot_id=5) is None


# ─── to_stream_requests ─────────────────────────────────────────────────

def test_translate_stop_emits_single_stop():
    msg = VideoRequest(action=VIDEO_ACTION_STOP)
    out = to_stream_requests(msg)
    assert out == [{"action": "stop"}]


def test_translate_start_emits_start_with_creds():
    msg = VideoRequest(
        action=VIDEO_ACTION_START,
        desired_port=5001, protocol="srt", codec="h265",
        quality="hd", encryption=True, passphrase_hint="abc",
    )
    out = to_stream_requests(msg)
    assert len(out) == 1
    assert out[0]["action"] == "start"
    creds = out[0]["creds"]
    assert creds["video_port"] == 5001
    assert creds["protocol"] == "srt"
    assert creds["codec"] == "h265"
    assert creds["quality"] == "hd"
    assert creds["encryption"] is True
    assert creds["passphrase_hint"] == "abc"


def test_translate_change_quality_emits_stop_then_start():
    msg = VideoRequest(
        action=VIDEO_ACTION_CHANGE_QUALITY,
        quality=VIDEO_QUALITY_LOW, desired_port=5002,
    )
    out = to_stream_requests(msg)
    assert len(out) == 2
    assert out[0] == {"action": "stop"}
    assert out[1]["action"] == "start"
    assert out[1]["creds"]["quality"] == VIDEO_QUALITY_LOW
    assert out[1]["creds"]["video_port"] == 5002


def test_translate_thumbnail_quality_round_trip():
    msg = VideoRequest(action="start", quality=VIDEO_QUALITY_THUMBNAIL)
    out = to_stream_requests(msg)
    assert out[0]["creds"]["quality"] == "thumbnail"
