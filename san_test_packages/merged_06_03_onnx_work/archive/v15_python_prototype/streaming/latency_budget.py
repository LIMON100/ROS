"""KPP-7 (real-time video latency ≤ 200 ms) — declared budget breakdown.

The end-to-end pipeline for follower → Hub → tablet has four serialised
contributions to glass-to-glass latency:

    encoder    : x264/x265 zerolatency tune + ultrafast preset
                 typical 25-35 ms on RK3588 software encoder
                 (mpph265enc hardware path is ~15 ms, used in prod)
    network    : SRT recv-side latency buffer
                 configured via build_srt_uri(..., latency_ms=...)
                 default 120 ms, designed to absorb Wi-Fi 6 jitter
    decoder    : ExoPlayer / GStreamer decode + present
                 typical 30-40 ms on modern Android tablets
    safety     : headroom for OS scheduler / clock-skew / mesh hop

This module is the **single source of truth** for those numbers, plus
the helper that lets a CI gate fail if a future change pushes the sum
past the 200 ms KPP. On-the-wire measurement is a follow-up integration
test that needs a real GStreamer + real network — captured separately
in `tests/test_video_latency_live.py` (skip-by-default, run with
`PATROL_RUN_LIVE_LATENCY=1`).
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict

# KPP-7 ceiling. SAN-IDS-CMD-001 v1.1 lists this as "실시간 영상 전송
# ≤ 200 ms" — glass-to-glass on the tablet.
KPP_LATENCY_BUDGET_MS = 200


@dataclass(frozen=True)
class LatencyComponent:
    """One contributor to the end-to-end latency budget."""
    name: str
    budget_ms: int
    description: str


# Declared breakdown. Tightening any value below the corresponding
# measured typical means we're claiming optimisation work; raising one
# eats the safety headroom. Changes here should be reflected in the
# CI gate test below.
LATENCY_BUDGET_BREAKDOWN: Dict[str, LatencyComponent] = {
    "encoder": LatencyComponent(
        name="encoder",
        budget_ms=35,
        description=(
            "x265enc/x264enc zerolatency on the follower's RK3588 "
            "software path (hardware mpph265enc is ~15 ms in prod)."),
    ),
    "srt_recv": LatencyComponent(
        name="srt_recv",
        budget_ms=120,
        description=(
            "SRT receiver-side buffer that absorbs Wi-Fi 6 mesh jitter "
            "+ ARQ retransmits. Matches DEFAULT_SRT_LATENCY_MS in "
            "streaming.gstreamer_relay."),
    ),
    "decoder": LatencyComponent(
        name="decoder",
        budget_ms=40,
        description=(
            "Tablet-side decode + compositor to screen. Typical "
            "ExoPlayer numbers on modern Android."),
    ),
    "safety": LatencyComponent(
        name="safety",
        budget_ms=5,
        description=(
            "Headroom for OS scheduler jitter, clock skew, extra mesh "
            "hops on a multi-AP deployment."),
    ),
}


def total_budget_ms() -> int:
    """Sum of every declared component."""
    return sum(c.budget_ms for c in LATENCY_BUDGET_BREAKDOWN.values())


def budget_remaining_ms() -> int:
    """How much KPP headroom the declared breakdown leaves.

    Positive = we have headroom; zero = exactly at the KPP; negative =
    the declared breakdown is already over budget and should not be
    deployed as-is. CI gates on `total_budget_ms() <= KPP_LATENCY_BUDGET_MS`.
    """
    return KPP_LATENCY_BUDGET_MS - total_budget_ms()


__all__ = (
    "KPP_LATENCY_BUDGET_MS",
    "LATENCY_BUDGET_BREAKDOWN",
    "LatencyComponent",
    "budget_remaining_ms",
    "total_budget_ms",
)
