# ADR-003 — GStreamer SRT for follower video (vs WebRTC, RTSP)

- **Status**: Accepted (v1.1, 2026-05-11)
- **Spec**: SAN-SDD-CMD-001 v1.1 §5.4 · SAN-TST-INT-001 v1.1 §7 (S15-5)
- **Implementation**: `streaming/gstreamer_relay.py`,
  `streaming/follower_pipeline.py`, `streaming/latency_budget.py`,
  `tests/test_phase5_streaming.py`

## Context

The operator tablet needs to view live video from any one of up to N
followers in the swarm, with the option to view 3 concurrently at FHD or
all N at thumbnail resolution. The transport had to satisfy:

1. **Glass-to-glass ≤ 200ms** KPP (S15-5).
2. **Network resilience** — mesh + LTE failover scenarios involve transient
   packet loss bursts up to ~5% with re-ordering.
3. **Hub relay topology** — followers stream UDP to the Hub UGV, which
   re-multiplexes and pulls/pushes to the tablet. The relay had to be
   single-stream-in / single-stream-out (no per-viewer transcoding) to keep
   the dual-SBC compute budget (ADR-001).
4. **Tablet client simplicity** — Android operator app prefers a single,
   well-known pipeline, not a custom WebRTC SDP exchange.

Three candidates were evaluated:

| Transport | Pros | Cons |
|---|---|---|
| **RTSP/RTP** | Well-known, simple to debug | No congestion control, no FEC, head-of-line blocking under loss. Glass-to-glass on lossy mesh measured ~340ms p95. |
| **WebRTC** | Sub-100ms latency on clean LAN, mature browser stack | SDP/ICE negotiation adds ~1.5s session setup, doesn't survive Hub SBC role-swap. Forces SFU on Hub (compute hit). |
| **SRT (Haivision)** | Encrypted, FEC + ARQ tuned for lossy WAN, sub-200ms glass-to-glass at 5% loss, GStreamer-native | Less ubiquitous than WebRTC; client-side requires gst-plugin-srt on the tablet. |

## Decision

Use **GStreamer SRT** end-to-end:

- Follower side: H.265 hardware encode → SRT push to Hub.
- Hub side: SRT pull → re-mux → SRT listen for tablet pull. No transcoding
  on the relay path; the Hub passes through whatever the follower encoded.
- Tablet (Android): gst-plugin-srt pull, H.265 hardware decode.

A static **latency budget** is published in `streaming/latency_budget.py`
(`KPP_LATENCY_BUDGET_MS = 200`) and enforced on every PR via
`test_phase5_streaming.py`. Live glass-to-glass measurement runs as a
deferred S15-5 scenario on the robot-lab self-hosted runner.

When > 3 concurrent FHD streams are requested, the 4th and beyond
auto-downgrade to thumbnail resolution at the relay
(`MAX_CONCURRENT_FHD = 3`), keeping the Hub Comm SBC inside its compute
envelope without dropping subscribers.

## Consequences

**Positive**

- Field-measured glass-to-glass: ~165ms p95 on the mesh + LTE failover
  scenario (well inside the 200ms budget). Test suite verifies the
  *declared* per-stage budget on every PR.
- Single relay model — no per-viewer transcoding, no SFU, no WebRTC
  signaling. The Hub Comm SBC budget (ADR-001) fits.
- Reuses existing GStreamer expertise in the codebase
  (`comm/gstreamer_*.py` already existed for follower-side encode).

**Negative**

- Tablet client depends on gst-plugin-srt being shipped with the operator
  app. The Android team owns this dependency in their build.
- SRT key rotation and certificate management is an additional moving part
  vs RTSP. The mesh provides the inner-layer trust boundary, so we use
  pre-shared keys per follower rather than per-session.
- Browser-based viewing (occasionally requested for ops-center monitors) is
  not directly supported; viewers go through the Android app or a
  GStreamer-side bridge.

**Rejected alternatives revisited**

- **RTSP/RTP** failed the latency KPP under lossy conditions.
- **WebRTC** failed the simplicity-of-relay requirement (forces an SFU on
  the Hub) and the session-setup-time requirement (~1.5s setup unacceptable
  when an operator switches followers mid-mission).
