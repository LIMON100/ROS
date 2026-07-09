"""Tests for PHASE 5 streaming additions (§5.1, §5.2, §5.4)."""
from typing import List

import pytest

from core.messages import (
    VIDEO_ACTION_START,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_QUALITY_FHD,
    VIDEO_QUALITY_HD,
    VIDEO_QUALITY_LOW,
    VIDEO_QUALITY_THUMBNAIL,
    VideoRequest,
)
from streaming.follower_pipeline import (
    QUALITY_PRESETS,
    build_follower_send_pipeline_argv,
    preset_for,
)
from streaming.gstreamer_relay import (
    MAX_CONCURRENT_FHD,
    GStreamerRelay,
)
from streaming.latency_budget import (
    KPP_LATENCY_BUDGET_MS,
    LATENCY_BUDGET_BREAKDOWN,
    budget_remaining_ms,
    total_budget_ms,
)

# ────────────────────────────────────────────────────────────────────────
# §5.4 — KPP-7 latency-budget gate
# ────────────────────────────────────────────────────────────────────────

def test_kpp7_total_budget_under_200ms():
    """The declared latency breakdown must fit inside the KPP-7 ceiling.

    Any future change that pushes a component up has to either offset
    it elsewhere (e.g. drop SRT latency from 120 → 80 ms after a mesh
    quality improvement) or accept that the KPP is at risk.
    """
    total = total_budget_ms()
    assert total <= KPP_LATENCY_BUDGET_MS, (
        f"declared latency total {total} ms exceeds KPP-7 ceiling "
        f"{KPP_LATENCY_BUDGET_MS} ms — review LATENCY_BUDGET_BREAKDOWN")


def test_kpp7_has_some_safety_headroom():
    # Headroom can be zero (exactly at KPP) but must never be negative.
    assert budget_remaining_ms() >= 0


def test_breakdown_components_have_positive_budgets():
    # A zero-budget component is a sign someone deleted a stage they
    # shouldn't have; catch it before merge.
    for name, comp in LATENCY_BUDGET_BREAKDOWN.items():
        assert comp.budget_ms > 0, f"component {name!r} has non-positive budget"


def test_breakdown_includes_required_stages():
    required = {"encoder", "srt_recv", "decoder"}
    assert required.issubset(LATENCY_BUDGET_BREAKDOWN)


# ────────────────────────────────────────────────────────────────────────
# §5.1 — follower send pipeline builder
# ────────────────────────────────────────────────────────────────────────

def test_preset_table_has_four_qualities():
    assert set(QUALITY_PRESETS) == {
        VIDEO_QUALITY_FHD, VIDEO_QUALITY_HD,
        VIDEO_QUALITY_LOW, VIDEO_QUALITY_THUMBNAIL,
    }


def test_preset_for_returns_record():
    p = preset_for(VIDEO_QUALITY_FHD)
    assert (p.width, p.height) == (1920, 1080)
    assert p.framerate == 30


def test_preset_for_rejects_unknown_quality():
    with pytest.raises(ValueError):
        preset_for("ultra")


def test_pipeline_starts_with_gst_launch():
    argv = build_follower_send_pipeline_argv(
        VIDEO_QUALITY_HD, VIDEO_CODEC_H265, "10.0.0.99", 5007)
    assert argv[0:3] == ["gst-launch-1.0", "-e", "-q"]


def test_pipeline_contains_required_elements():
    argv = build_follower_send_pipeline_argv(
        VIDEO_QUALITY_HD, VIDEO_CODEC_H265, "10.0.0.99", 5007)
    joined = " ".join(argv)
    # End-to-end shape the Hub relay (udpsrc!rtpmp2tdepay!tsparse!srtsink)
    # expects on the wire.
    for needle in ("v4l2src", "videoconvert", "x265enc",
                   "tune=zerolatency", "speed-preset=ultrafast",
                   "h265parse", "mpegtsmux", "rtpmp2tpay",
                   "udpsink", "host=10.0.0.99", "port=5007"):
        assert needle in joined, f"missing {needle!r} in pipeline argv"


def test_pipeline_uses_h264_path_for_h264_codec():
    argv = build_follower_send_pipeline_argv(
        VIDEO_QUALITY_HD, VIDEO_CODEC_H264, "10.0.0.99", 5007)
    joined = " ".join(argv)
    assert "x264enc" in joined
    assert "h264parse" in joined
    assert "x265enc" not in joined


def test_pipeline_bakes_in_preset_resolution_and_bitrate():
    argv = build_follower_send_pipeline_argv(
        VIDEO_QUALITY_LOW, VIDEO_CODEC_H265, "10.0.0.99", 5007)
    joined = " ".join(argv)
    p = QUALITY_PRESETS[VIDEO_QUALITY_LOW]
    assert f"width={p.width}" in joined
    assert f"height={p.height}" in joined
    assert f"framerate={p.framerate}/1" in joined
    assert f"bitrate={p.bitrate_kbps}" in joined


def test_pipeline_rejects_unknown_quality():
    with pytest.raises(ValueError):
        build_follower_send_pipeline_argv(
            "ultra", VIDEO_CODEC_H265, "10.0.0.99", 5007)


def test_pipeline_rejects_unknown_codec():
    with pytest.raises(ValueError):
        build_follower_send_pipeline_argv(
            VIDEO_QUALITY_HD, "av1", "10.0.0.99", 5007)


def test_pipeline_rejects_empty_hub_ip():
    with pytest.raises(ValueError):
        build_follower_send_pipeline_argv(
            VIDEO_QUALITY_HD, VIDEO_CODEC_H265, "", 5007)


@pytest.mark.parametrize("bad_port", [0, -1, 65_536, 100_000])
def test_pipeline_rejects_out_of_range_port(bad_port):
    with pytest.raises(ValueError):
        build_follower_send_pipeline_argv(
            VIDEO_QUALITY_HD, VIDEO_CODEC_H265, "10.0.0.99", bad_port)


# ────────────────────────────────────────────────────────────────────────
# §5.2 — MAX_CONCURRENT_FHD auto-downgrade
# ────────────────────────────────────────────────────────────────────────

class _FakePopen:
    instances: List["_FakePopen"] = []

    def __init__(self, argv):
        self.argv = argv
        self.terminated = False
        _FakePopen.instances.append(self)

    def terminate(self):
        self.terminated = True


@pytest.fixture(autouse=True)
def _reset_fake_popen():
    _FakePopen.instances.clear()
    yield
    _FakePopen.instances.clear()


def _fake_factory(argv):
    return _FakePopen(argv)


def _make_relay():
    return GStreamerRelay(
        hub_external_ip="10.0.0.99",
        popen_factory=_fake_factory,
        token_hex=lambda n: "ab" * n,
    )


def _fhd_request(robot_id: int, quality: str = VIDEO_QUALITY_FHD,
                 sequence: int = 1) -> VideoRequest:
    return VideoRequest(
        sequence=sequence, target_robot_id=robot_id,
        protocol="srt", desired_port=0,
        codec=VIDEO_CODEC_H265, quality=quality,
        encryption=True, passphrase_hint="",
        action=VIDEO_ACTION_START,
        timestamp_ms=1_700_000_000_000,
    )


def test_max_concurrent_constant_is_three():
    assert MAX_CONCURRENT_FHD == 3


def test_below_cap_keeps_requested_quality():
    relay = _make_relay()
    for rid in (1, 2, 3):
        resp = relay.start(_fhd_request(rid), now_ms=1_000)
        assert resp.quality == VIDEO_QUALITY_FHD


def test_fourth_fhd_auto_downgrades_to_thumbnail():
    relay = _make_relay()
    for rid in (1, 2, 3):
        relay.start(_fhd_request(rid), now_ms=1_000)
    # The 4th FHD request crosses MAX_CONCURRENT_FHD — gets downgraded.
    resp = relay.start(_fhd_request(robot_id=4), now_ms=1_000)
    assert resp.quality == VIDEO_QUALITY_THUMBNAIL
    # The cached handle reflects the effective quality too — so a later
    # response_for() doesn't surprise the tablet with a different value.
    h = relay.handle_for(4)
    assert h.quality == VIDEO_QUALITY_THUMBNAIL


def test_hd_also_counts_against_cap():
    # The cap is "heavy qualities" (HD + FHD), not just FHD. So three
    # HD streams should also trigger the downgrade for a fourth HD.
    relay = _make_relay()
    for rid in (1, 2, 3):
        relay.start(_fhd_request(rid, quality=VIDEO_QUALITY_HD),
                    now_ms=1_000)
    resp = relay.start(_fhd_request(4, quality=VIDEO_QUALITY_HD),
                       now_ms=1_000)
    assert resp.quality == VIDEO_QUALITY_THUMBNAIL


def test_low_and_thumbnail_dont_count_against_cap():
    # Three low-quality streams should NOT push an HD request to
    # downgrade — only HD/FHD are "heavy."
    relay = _make_relay()
    for rid in (1, 2, 3):
        relay.start(_fhd_request(rid, quality=VIDEO_QUALITY_LOW),
                    now_ms=1_000)
    resp = relay.start(_fhd_request(4, quality=VIDEO_QUALITY_HD),
                       now_ms=1_000)
    assert resp.quality == VIDEO_QUALITY_HD


def test_replacing_existing_robot_bypasses_cap():
    # Replacing a robot's own pipeline (re-asking for a stream the
    # tablet already has) shouldn't accumulate against the cap — the
    # old slot terminates before the new one spawns.
    relay = _make_relay()
    for rid in (1, 2, 3):
        relay.start(_fhd_request(rid), now_ms=1_000)
    # Robot 1 re-asks at FHD — still FHD because we're replacing.
    resp = relay.start(_fhd_request(1, sequence=2), now_ms=2_000)
    assert resp.quality == VIDEO_QUALITY_FHD


def test_low_request_never_downgrades_even_under_load():
    # Low/thumbnail requests are never touched by the cap.
    relay = _make_relay()
    for rid in (1, 2, 3):
        relay.start(_fhd_request(rid), now_ms=1_000)
    resp = relay.start(_fhd_request(4, quality=VIDEO_QUALITY_LOW),
                       now_ms=1_000)
    assert resp.quality == VIDEO_QUALITY_LOW
