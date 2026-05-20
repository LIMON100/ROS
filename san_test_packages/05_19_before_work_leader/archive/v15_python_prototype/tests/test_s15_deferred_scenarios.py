"""S15-2, S15-5, S15-6 — deferred scenarios needing live hardware/sim.

The spec's SAN-TST-INT-001 v1.1 §7 names six S15 scenarios. Three of
them (1, 3, 4) are implemented as pure-Python integration tests on
the merged modules — see test_s15_{1,3,4}_*.py. The remaining three
require infrastructure that doesn't live in this Python codebase:

  S15-2  Pan-tilt sweep detection KPP (≥90%)
         → needs a live AI detector + a moving target on Gazebo W2 forest;
           the YOLOv5 inference path lives in perception/, but the
           detection-RATE measurement is the AI model's metric, not a
           controller-logic property we can assert here.

  S15-5  GStreamer SRT latency KPP (≤200 ms)
         → glass-to-glass measurement across a real Wi-Fi / LTE link
           with timeoverlay frames. The DECLARED budget is enforced
           on every PR (test_phase5_streaming.py); the LIVE
           measurement runs on a self-hosted robot-lab runner under
           workflow_dispatch.

  S15-6  Multi-follower simultaneous video monitoring
         → needs the Android operator app + N real followers. The
           Hub-side relay's MAX_CONCURRENT_FHD downgrade behaviour
           is unit-tested in test_phase5_streaming.py.

This file exists so the S15 ledger shows up green-or-skipped in CI
output — never silently absent. Skipped tests print their environment
requirement in the reason string.
"""
from __future__ import annotations

import os

import pytest

# ─── flags that switch a skip to a live run ────────────────────────────
# Set these in a self-hosted runner's env to actually exercise the
# scenario; on github-hosted runners they stay False and the tests
# skip with a clear reason.
_LIVE_PERCEPTION = os.environ.get("PATROL_S15_LIVE_PERCEPTION") == "1"
_LIVE_VIDEO_LAT  = os.environ.get("PATROL_S15_LIVE_VIDEO_LATENCY") == "1"
_LIVE_MULTI_VID  = os.environ.get("PATROL_S15_LIVE_MULTI_VIDEO") == "1"


# ─── S15-2 ─────────────────────────────────────────────────────────────

@pytest.mark.skipif(
    not _LIVE_PERCEPTION,
    reason=(
        "S15-2 detection-rate KPP needs Gazebo W2 forest + live YOLOv5 "
        "inference. Set PATROL_S15_LIVE_PERCEPTION=1 on a self-hosted "
        "GPU runner to exercise. PR #43 ships the pan-tilt sweep math; "
        "the AI detection rate is the model's metric, not a controller "
        "invariant we can assert in pytest."))
def test_s15_2_pantilt_sweep_detection_rate_at_least_90pct():
    raise NotImplementedError("see skip reason")


# ─── S15-5 ─────────────────────────────────────────────────────────────

@pytest.mark.skipif(
    not _LIVE_VIDEO_LAT,
    reason=(
        "S15-5 SRT-glass-to-glass latency needs a live GStreamer "
        "pipeline + a real Wi-Fi/LTE link + a tablet running the "
        "operator app. The DECLARED budget (encoder + SRT + decoder "
        "+ safety ≤ 200 ms) is enforced on every PR by "
        "test_phase5_streaming.py — see KPP_LATENCY_BUDGET_MS. The "
        "LIVE measurement runs under workflow_dispatch in the "
        "robot-lab regression workflow. Set "
        "PATROL_S15_LIVE_VIDEO_LATENCY=1 to opt in."))
def test_s15_5_gstreamer_srt_glass_to_glass_under_200ms():
    raise NotImplementedError("see skip reason")


# ─── S15-6 ─────────────────────────────────────────────────────────────

@pytest.mark.skipif(
    not _LIVE_MULTI_VID,
    reason=(
        "S15-6 multi-follower video monitoring needs the Android "
        "operator app + N real followers streaming through the Hub "
        "relay. The MAX_CONCURRENT_FHD=3 downgrade BEHAVIOR is "
        "unit-tested in test_phase5_streaming.py "
        "(test_fourth_fhd_auto_downgrades_to_thumbnail). The "
        "end-to-end monitoring story is for the field-test phase."))
def test_s15_6_multi_follower_video_concurrent_streaming():
    raise NotImplementedError("see skip reason")


# ─── meta: deferred-test ledger is complete ───────────────────────────

def test_s15_scenario_ledger_is_complete():
    """Sanity: this file must hold a stub for each of the three
    deferred scenarios. If a future cleanup deletes one, this fails
    so the S15 ledger doesn't silently shrink.
    """
    import sys
    from inspect import getmembers, isfunction
    mod = sys.modules[__name__]
    s15_tests = [
        name for name, _ in getmembers(mod, isfunction)
        if name.startswith("test_s15_")]
    assert {"test_s15_2_pantilt_sweep_detection_rate_at_least_90pct",
            "test_s15_5_gstreamer_srt_glass_to_glass_under_200ms",
            "test_s15_6_multi_follower_video_concurrent_streaming",
            } <= set(s15_tests)
