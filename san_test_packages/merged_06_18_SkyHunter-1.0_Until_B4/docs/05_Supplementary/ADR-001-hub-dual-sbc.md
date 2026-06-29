# ADR-001 — Hub UGV dual-SBC architecture

- **Status**: Accepted (v1.1, 2026-05-11)
- **Spec**: SAN-SDD-CMD-001 v1.1 §6 · SAN-IDS-CMD-001 v1.1 (SwarmHealthSummary)
- **Implementation**: `adapters/hub_ugv.py`, `safety/hub_health_monitor.py`,
  `tests/test_s15_3_hub_dual_sbc.py`

## Context

In v1.0 the Hub UGV ran on a single RK3588J SBC. That node hosted:

- 8-follower SLAM tile fusion (`MapFusionProcess`, aggregator)
- GStreamer relay for follower video (4 concurrent FHD targets)
- Mesh router controller / `MeshMonitor` polling
- Aggregated-map broadcast publisher
- Leadership-takeover state machine

A single SBC failure took the entire swarm coordination layer offline. Field
runs in late v1.0 also revealed compute contention: SLAM fusion bursts (every
30s once we landed ADR-002) collided with GStreamer encode/forward, causing
video frame drops during the highest-stress moments (formation regroup or
threat-focus dispatch).

Two pressures pushed us toward separating concerns:

1. **Reliability** — losing video and SLAM together is worse than losing
   either one. A mission can degrade gracefully if comm/video survives a SLAM
   crash, or if SLAM survives a video crash.
2. **Compute headroom** — once narrow-mode SLAM at 15s cadence (ADR-002) and
   3-stream FHD relay (PHASE 5) are simultaneously active, a single RK3588J
   is pinned > 90% sustained.

## Decision

Adopt a **dual-SBC** layout for the Hub UGV:

- **SLAM SBC** (`san_hub_slam`) — runs the aggregator, MapFusionProcess,
  AggregatedMap publisher.
- **Comm/Video SBC** (`san_hub_comm`) — runs the GStreamer relay, mesh
  controller, threat alert routing, leadership-takeover state machine.

The two SBCs exchange peer heartbeats. `HubHealthMonitor` latches a 3-second
timeout per peer and emits a `SwarmHealthSummary` with `slam_sbc_failed` /
`comm_sbc_failed` flags + a `ThreatAlert(SBC_FAILED, WARNING)` edge event for
the operator banner. When one SBC drops, the other continues running its own
subset of duties (no automatic role swap — explicit partial-operation mode).

The BOM allows two physical realizations: **2× RK3588J** (parity with v1.0
parts) or **1× Jetson Orin Nano** (sufficient for both roles in a single
node, kept as the fallback path until dual-SBC enclosures are produced).

## Consequences

**Positive**

- Single-SBC failure no longer takes the swarm coordination layer down.
  S15-3 scenario validates partial-operation paths end-to-end.
- Compute envelope per SBC drops below 60% sustained, leaving headroom for
  future YOLOv8 upgrade and additional follower count.
- Clear ownership boundary: SLAM team owns `san_hub_slam`, comm team owns
  `san_hub_comm` (mirrors team structure).

**Negative**

- BOM cost +1 SBC per Hub UGV (~$200 in RK3588J configuration; $0 if Orin
  Nano variant since it replaces, not adds).
- Cross-SBC inter-process state (acting-leader flag, peer roster) now needs
  explicit synchronization via the heartbeat protocol — previously implicit.
- Test surface grows: every SwarmHealthSummary-producing path must be
  validated under both peer-alive and partial-operation conditions
  (`test_s15_3_hub_dual_sbc.py` 7-test suite).

**Migration**

- v1.0 single-SBC `san_hub` package is deprecated; v1.2 will remove it.
- Field units already deployed run the Orin Nano variant until they are
  rotated through service for the dual-RK3588J enclosure swap.
