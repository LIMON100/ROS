"""Tests for streaming.gstreamer_relay — Hub UGV SBC#2 video relay."""
import pytest

from core.messages import (
    VIDEO_ACTION_CHANGE_QUALITY,
    VIDEO_ACTION_START,
    VIDEO_ACTION_STOP,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_STATUS_STOPPED,
    VIDEO_STATUS_STREAMING,
    VideoRequest,
)
from streaming.gstreamer_relay import (
    BASE_SRT_PORT_HUB_TO_TABLET,
    BASE_UDP_PORT_FOLLOWER_TO_HUB,
    BITRATE_KBPS_BY_QUALITY,
    DEFAULT_SRT_LATENCY_MS,
    PASSPHRASE_BYTES,
    GStreamerRelay,
    build_follower_start_cmd,
    build_relay_pipeline_argv,
    build_srt_uri,
    generate_passphrase,
    srt_port_for,
    udp_port_for,
)

# ─── fakes ─────────────────────────────────────────────────────────────

class _FakePopen:
    """Test double for subprocess.Popen — records argv, tracks terminate."""
    instances: list = []

    def __init__(self, argv):
        self.argv = argv
        self.terminated = False
        self.returncode = None
        _FakePopen.instances.append(self)

    def terminate(self):
        self.terminated = True
        self.returncode = 0


@pytest.fixture(autouse=True)
def _reset_fake_popen():
    _FakePopen.instances.clear()
    yield
    _FakePopen.instances.clear()


def _fake_factory(argv):
    return _FakePopen(argv)


def _deterministic_token_hex(nbytes: int) -> str:
    return "ab" * nbytes  # 32 hex chars when nbytes == 16


def _make_relay(**kwargs):
    return GStreamerRelay(
        hub_external_ip=kwargs.pop("hub_external_ip", "10.0.0.99"),
        popen_factory=kwargs.pop("popen_factory", _fake_factory),
        token_hex=kwargs.pop("token_hex", _deterministic_token_hex),
        **kwargs,
    )


def _start_request(robot_id=7, codec=VIDEO_CODEC_H265, quality=VIDEO_QUALITY_HD,
                   sequence=42):
    return VideoRequest(
        sequence=sequence,
        target_robot_id=robot_id,
        protocol="srt",
        desired_port=0,
        codec=codec,
        quality=quality,
        encryption=True,
        passphrase_hint="",
        action=VIDEO_ACTION_START,
        timestamp_ms=1_700_000_000_000,
    )


# ─── port helpers ──────────────────────────────────────────────────────

def test_udp_port_offsets_from_base():
    assert udp_port_for(0) == BASE_UDP_PORT_FOLLOWER_TO_HUB
    assert udp_port_for(7) == BASE_UDP_PORT_FOLLOWER_TO_HUB + 7


def test_srt_port_offsets_from_base():
    assert srt_port_for(0) == BASE_SRT_PORT_HUB_TO_TABLET
    assert srt_port_for(7) == BASE_SRT_PORT_HUB_TO_TABLET + 7


def test_udp_port_rejects_negative_robot_id():
    with pytest.raises(ValueError):
        udp_port_for(-1)


def test_srt_port_rejects_negative_robot_id():
    with pytest.raises(ValueError):
        srt_port_for(-1)


def test_udp_port_rejects_overflow():
    with pytest.raises(ValueError):
        udp_port_for(100_000)


# ─── passphrase ────────────────────────────────────────────────────────

def test_generate_passphrase_length():
    pp = generate_passphrase()
    # 16 random bytes → 32 hex chars
    assert len(pp) == PASSPHRASE_BYTES * 2
    int(pp, 16)  # must parse as hex


def test_generate_passphrase_random_by_default():
    a = generate_passphrase()
    b = generate_passphrase()
    assert a != b


def test_generate_passphrase_deterministic_with_injected_rng():
    pp = generate_passphrase(token_hex=_deterministic_token_hex)
    assert pp == "ab" * PASSPHRASE_BYTES


# ─── SRT URI builder ───────────────────────────────────────────────────

def test_build_srt_uri_minimal():
    uri = build_srt_uri("10.0.0.5", 8888, "listener")
    assert uri == f"srt://10.0.0.5:8888?mode=listener&latency={DEFAULT_SRT_LATENCY_MS}"


def test_build_srt_uri_with_passphrase_and_streamid():
    uri = build_srt_uri(
        "10.0.0.5", 8895, "caller",
        latency_ms=200, passphrase="deadbeef", streamid="robot7-h265")
    assert uri == (
        "srt://10.0.0.5:8895?mode=caller&latency=200"
        "&passphrase=deadbeef&streamid=robot7-h265")


def test_build_srt_uri_rejects_unknown_mode():
    with pytest.raises(ValueError):
        build_srt_uri("10.0.0.5", 8888, "rendezvous")


# ─── pipeline argv ─────────────────────────────────────────────────────

def test_pipeline_argv_starts_with_gst_launch():
    argv = build_relay_pipeline_argv(
        udp_port=5007, srt_uri="srt://0.0.0.0:8895?mode=listener&latency=120",
        codec=VIDEO_CODEC_H265)
    assert argv[0:3] == ["gst-launch-1.0", "-e", "-q"]


def test_pipeline_argv_contains_required_elements():
    argv = build_relay_pipeline_argv(
        udp_port=5007, srt_uri="srt://x:8895?mode=listener", codec="h265")
    joined = " ".join(argv)
    # Spec elements: udpsrc → rtpmp2tdepay → tsparse → srtsink
    assert "udpsrc" in joined
    assert "port=5007" in joined
    assert "rtpmp2tdepay" in joined
    assert "tsparse" in joined
    assert "srtsink" in joined
    assert "uri=srt://x:8895?mode=listener" in joined


def test_pipeline_argv_accepts_h264_and_h265():
    for codec in (VIDEO_CODEC_H264, VIDEO_CODEC_H265):
        build_relay_pipeline_argv(
            udp_port=5000, srt_uri="srt://x:8888?mode=listener", codec=codec)


def test_pipeline_argv_rejects_unknown_codec():
    with pytest.raises(ValueError):
        build_relay_pipeline_argv(
            udp_port=5000, srt_uri="srt://x:8888?mode=listener", codec="av1")


# ─── follower start cmd ────────────────────────────────────────────────

def test_follower_start_cmd_targets_hub_udp_port():
    req = _start_request(robot_id=7)
    cmd = build_follower_start_cmd(req, udp_port=5007, hub_ip="10.0.0.99")
    assert cmd["action"] == "start"
    creds = cmd["creds"]
    assert creds["video_port"] == 5007
    assert creds["host"] == "10.0.0.99"
    assert creds["protocol"] == "udp"
    assert creds["codec"] == VIDEO_CODEC_H265
    assert creds["quality"] == VIDEO_QUALITY_HD


# ─── GStreamerRelay state machine ──────────────────────────────────────

def test_constructor_rejects_empty_hub_ip():
    with pytest.raises(ValueError):
        GStreamerRelay(hub_external_ip="")


def test_start_spawns_subprocess_and_records_handle():
    relay = _make_relay()
    resp = relay.start(_start_request(robot_id=7), now_ms=1_000)
    assert relay.is_active(7)
    assert len(_FakePopen.instances) == 1
    # Response carries the public (external-IP) SRT URI in caller mode.
    assert resp.status == VIDEO_STATUS_STREAMING
    assert "10.0.0.99" in resp.srt_uri
    assert "mode=caller" in resp.srt_uri
    assert resp.passphrase == "ab" * PASSPHRASE_BYTES


def test_start_response_carries_quality_bitrate_and_sequence():
    relay = _make_relay()
    resp = relay.start(_start_request(quality=VIDEO_QUALITY_FHD, sequence=99),
                        now_ms=1_000)
    assert resp.actual_bitrate_kbps == BITRATE_KBPS_BY_QUALITY[VIDEO_QUALITY_FHD]
    assert resp.sequence == 99
    assert resp.stream_start_ms == 1_000


def test_start_pipeline_argv_uses_listener_mode_with_passphrase():
    relay = _make_relay()
    relay.start(_start_request(robot_id=7), now_ms=1_000)
    argv = _FakePopen.instances[0].argv
    joined = " ".join(argv)
    # Hub binds 0.0.0.0 listener — distinct from the caller URI returned
    # to the tablet.
    assert "0.0.0.0" in joined
    assert "mode=listener" in joined
    assert ("passphrase=" + "ab" * PASSPHRASE_BYTES) in joined


def test_start_twice_replaces_old_pipeline():
    relay = _make_relay()
    relay.start(_start_request(robot_id=7), now_ms=1_000)
    first = _FakePopen.instances[0]
    relay.start(_start_request(robot_id=7), now_ms=2_000)
    # Old subprocess must be terminated; new one alive.
    assert first.terminated is True
    assert len(_FakePopen.instances) == 2
    assert _FakePopen.instances[1].terminated is False
    assert relay.is_active(7)


def test_stop_terminates_pipeline_and_returns_stopped_response():
    relay = _make_relay()
    relay.start(_start_request(robot_id=7), now_ms=1_000)
    stop_req = _start_request(robot_id=7)
    stop_req.action = VIDEO_ACTION_STOP
    resp = relay.stop(7, now_ms=2_000, last_request=stop_req)
    assert _FakePopen.instances[0].terminated is True
    assert relay.is_active(7) is False
    assert resp.status == VIDEO_STATUS_STOPPED
    assert resp.passphrase == ""           # no secret when not streaming
    assert resp.stream_start_ms == 0


def test_stop_on_nonexistent_robot_is_safe():
    relay = _make_relay()
    resp = relay.stop(99, now_ms=1_000)
    assert resp.status == VIDEO_STATUS_STOPPED
    assert relay.is_active(99) is False


def test_change_quality_replaces_pipeline():
    relay = _make_relay()
    relay.start(_start_request(robot_id=7, quality=VIDEO_QUALITY_HD),
                now_ms=1_000)
    change = _start_request(robot_id=7, quality=VIDEO_QUALITY_FHD)
    change.action = VIDEO_ACTION_CHANGE_QUALITY
    resp = relay.change_quality(change, now_ms=2_000)
    # Old pipeline killed, new one in its place.
    assert _FakePopen.instances[0].terminated is True
    assert relay.is_active(7)
    assert resp.actual_bitrate_kbps == BITRATE_KBPS_BY_QUALITY[VIDEO_QUALITY_FHD]


def test_shutdown_terminates_all():
    relay = _make_relay()
    relay.start(_start_request(robot_id=1), now_ms=1_000)
    relay.start(_start_request(robot_id=2), now_ms=1_000)
    relay.start(_start_request(robot_id=3), now_ms=1_000)
    assert relay.active_robots == [1, 2, 3]
    relay.shutdown()
    assert relay.active_robots == []
    assert all(p.terminated for p in _FakePopen.instances)


def test_shutdown_idempotent():
    relay = _make_relay()
    relay.start(_start_request(robot_id=5), now_ms=1_000)
    relay.shutdown()
    # Second call: nothing left, no-op.
    relay.shutdown()
    assert relay.active_robots == []


# ─── on_video_request dispatch ─────────────────────────────────────────

def test_on_video_request_start():
    relay = _make_relay()
    resp = relay.on_video_request(_start_request(robot_id=7), now_ms=1_000)
    assert resp.status == VIDEO_STATUS_STREAMING


def test_on_video_request_stop():
    relay = _make_relay()
    relay.on_video_request(_start_request(robot_id=7), now_ms=1_000)
    stop = _start_request(robot_id=7)
    stop.action = VIDEO_ACTION_STOP
    resp = relay.on_video_request(stop, now_ms=2_000)
    assert resp.status == VIDEO_STATUS_STOPPED
    assert relay.is_active(7) is False


def test_on_video_request_change_quality():
    relay = _make_relay()
    relay.on_video_request(
        _start_request(robot_id=7, quality=VIDEO_QUALITY_HD), now_ms=1_000)
    change = _start_request(robot_id=7, quality=VIDEO_QUALITY_FHD)
    change.action = VIDEO_ACTION_CHANGE_QUALITY
    resp = relay.on_video_request(change, now_ms=2_000)
    assert resp.quality == VIDEO_QUALITY_FHD
    assert resp.actual_bitrate_kbps == BITRATE_KBPS_BY_QUALITY[VIDEO_QUALITY_FHD]


# ─── handle introspection ──────────────────────────────────────────────

def test_handle_for_returns_running_state():
    relay = _make_relay()
    relay.start(_start_request(robot_id=7), now_ms=1_000)
    h = relay.handle_for(7)
    assert h is not None
    assert h.robot_id == 7
    assert h.udp_port == udp_port_for(7)
    assert h.srt_port == srt_port_for(7)
    assert h.quality == VIDEO_QUALITY_HD


def test_handle_for_nonexistent_returns_none():
    relay = _make_relay()
    assert relay.handle_for(99) is None
