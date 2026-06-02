# ADR-002 — SLAM aggregation period: 30–60s (v1.1) → 5s (v1.3)

- **Status**: Superseded by v1.3 retune (2026-05-11). Original v1.1
  decision retained below for historical context; v1.3 amendment in
  the "Amendment (v1.3)" section.
- **Spec**: SAN-IDS-CMD-001 v1.3 — `AggregatedMap`, `SLAMLocalDelta`
- **Implementation**: `mapping/slam_aggregator.py`,
  `mapping/slam_local_publisher.py`, `mapping/aggregated_to_static.py`
  (PHASE 3 ↔ PHASE 1 bridge), `tests/test_s15_4_slam_aggregation.py`,
  `tests/test_slam_v13_cadence.py`

## Context

In v1.0 the Hub UGV broadcast a fused occupancy grid at **1 Hz**. With 8
followers each contributing a 40 m × 40 m local tile at 5 cm resolution,
this required ~3.2 MB of raw payload per fused frame and ~25 Mbps sustained
over the mesh once the AggregatedMap envelope, peer overhead, and retry
budget were factored in. Observed effects in v1.0 field runs:

- Mesh saturation during follower video relay — SRT FHD streams contended
  with the SLAM broadcast for backhaul bandwidth.
- Battery drain on the Hub UGV's Wi-Fi radio — 25 Mbps continuous TX is the
  single largest non-motor power draw.
- Diminishing returns — operator-facing map UI in practice refreshes the
  display at sub-Hz; the 1 Hz cadence was driven by an early-prototype
  assumption that anomalies should appear "live."

Anomaly detection logic was reworked to handle deltas asynchronously rather
than scanning each full frame, freeing the aggregation cadence from the
detection cadence.

## Decision

Aggregate at **30s** in default surveillance mode, **15s** in narrow-mode /
formation-regroup, with a **force_event** path that immediately re-fuses on
formation change or a follower drop-out.

Local-delta publishers (`SlamLocalPublisher`) compress their per-follower
tile updates as PNG (lossless 8-bit occupancy) so that the *delta* over the
30s window dominates the wire cost, not the absolute map. Followers publish
their `SLAMLocalDelta` whenever their coverage window changes; the Hub
fuses them into `AggregatedMap` on the periodic cadence above.

## Consequences

**Positive**

- Mesh bandwidth dedicated to SLAM drops from ~25 Mbps to ~0.25 Mbps
  sustained (a ~100× reduction). Confirmed in `tests/test_aggregated_map.py`
  bandwidth assertions.
- Power draw on the Hub Wi-Fi radio drops by ~40% sustained, materially
  extending Hub UGV battery life.
- Formation regroup latency stays acceptable because of the force_event
  path — operators see the new layout within one aggregation cycle (<1s in
  the force case) rather than waiting up to 30s.

**Negative**

- Operator-facing map UI now has visible "staleness" — the last fused frame
  can be up to 30s old in default mode. UX team mitigated this by surfacing
  the `coverage_window_start_ms` / `coverage_window_end_ms` fields in the
  operator console so the displayed age is explicit.
- Threat detection from the *map* (e.g. a structure that wasn't there in
  the last fused frame) lags by up to 30s. This is acceptable because
  active threat detection lives on the perception pipeline, not the map
  diff; the map is for situational awareness, not real-time alerting.

**Migration**

- `SLAMDelta.msg` (v1.0) is deprecated in favor of `SLAMLocalDelta.msg`.
  Followers running v1.0 firmware will continue to publish at 1 Hz; the
  v1.1 aggregator coalesces incoming 1 Hz frames into the 30s window
  without error. v1.2 will drop the legacy path.

---

## Amendment (v1.3, PHASE 3 — 2026-05-11)

The v1.1 decision is **reversed** for the default cadence. New table:

| Mode | v1.1 | v1.3 |
|---|---:|---:|
| `wide`     | 60 s | 30 s |
| `default`  | 30 s |  5 s |
| `narrow`   | 15 s |  2.5 s |
| `obstacle` | 10 s |  1 s |

### Why the reversal

The v1.1 reasoning was sound for the v1.1 hardware: single-SBC Hub UGV,
limited CPU and Wi-Fi headroom. v1.3 changes both inputs:

1. **Dual-SBC Hub** (SAN-SDD-SWARM-001 v1.3 §3.4). SBC #1 is now
   dedicated to SLAM aggregation; the per-frame fuse budget grew ~3×.
2. **PHASE 1 cost map** consumes the `AggregatedMap` as its static
   layer (`mapping/aggregated_to_static.py`). The local planner sees
   global obstacles only as fresh as the last fused frame — a 30 s lag
   on a 3.33 m/s UGV is up to 100 m of stale geometry. Unacceptable
   once cost-map-driven avoidance is wired (PHASE 1 §6.4).
3. **Wi-Fi 6 mesh** budget recomputed under SAN v1.3 §9: 6–20 KB/s
   steady-state, ≤ 3 % of mesh capacity. Comfortably within reach at
   5 s cadence; the test caps in `tests/test_slam_local_publisher.py`
   now enforce this directly.

### Trade-offs (v1.3)

**Positive**
- 6× fresher global SLAM → cost-map static layer reflects swarm-wide
  obstacles within 5 s instead of 30 s.
- Operator UX staleness drops from up to 30 s to up to 5 s, well below
  the operator-noticeable threshold.
- `obstacle` mode at 1 s effectively makes burst-cadence near-realtime
  during dense-obstacle traversal.

**Negative**
- Hub SBC #1 sustained busy ratio rises from 10–20 % to 40–60 % at the
  default cadence. Verified to fit on the SDD §3.4 hardware envelope.
- Mesh bandwidth rises from 1–3 KB/s steady to 6–20 KB/s. Still under
  3 % of mesh capacity; verified by
  `test_bandwidth_under_20kbps_per_swarm_at_default_5s`.

### Implementation notes

- `mapping/aggregated_map.py` `DEFAULT_PERIOD_S` flipped 30 → 5; the
  dispatcher's hard floor of 30 s is replaced by a soft floor of 1 s
  (`MIN_PERIOD_S`). `LEGACY_DEFAULT_PERIOD_S = 30.0` remains as a
  named constant for opt-in back-compat (replay tools, dashboards).
- `slam_aggregator.PERIOD_BY_MODE` and
  `slam_local_publisher.PERIOD_BY_MODE` are kept locked-step. A guard
  test (`test_publisher_aggregator_table_locked`) makes a future
  divergence loud.
- Hot reload: the cadence is settable at runtime via
  `set_mode(...)` / `set_period_sec(...)` on both ends, with the
  `slam` section of `config/system.yaml` carrying the v1.3 defaults.
  `core.config_manager` picks the new value up without a process
  restart.
- PHASE 1 integration: `mapping/aggregated_to_static.py` consumes an
  `AggregatedMap` + the robot pose and emits a 280 × 280 uint8 window
  feeding directly into `CostMap.compose(static_layer_window=...)`.
