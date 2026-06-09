"""S15-4 — SLAM 5 s aggregation consistency scenario (SAN v1.3 §9).

Spec: SAN-TST-INT-001 v1.3 §S15-4 (updated from v1.1's 30-60 s).
Spec target: global-map registration error ≤ 0.5 m on W3 urban_blocks.

Scenario-level integration test wiring the follower → Hub fusion path:

    SlamLocalPublisher (N followers, 1 Hz ingest)
            ↓
        SLAMLocalDelta
            ↓
    SlamAggregator (Hub SBC#1, 5 s default cadence — v1.3)
            ↓
        AggregatedMap

Walks through:

  * N=3 followers publishing disjoint local grids → aggregated canvas
    covers the union bbox, contributing_robots = 3
  * N=3 followers publishing OVERLAPPING grids with conflicting
    obstacle cells → per-pixel max wins; no observation is lost
  * The 5-s default cadence (v1.3) holds: aggregator goes quiet
    between publishes, fires again at the boundary
  * mode switch (default → narrow → 2.5 s) shortens the cadence
  * Registration-error proxy: when two followers report the SAME
    obstacle at the same global coordinate, the aggregated cell at
    that coordinate is still occupied (no shift, no loss)

Out-of-scope vs the Gazebo S15-4: real pose-graph optimisation across
robots (the optimize_pose_graph stub is a pass-through pending a
SLAMLocalDelta pose-graph payload).
"""
from __future__ import annotations

import numpy as np

from core.messages import Pose2D
from mapping.aggregated_map import decode_png_to_grid, encode_grid_to_png
from mapping.slam_aggregator import (
    MODE_DEFAULT,
    MODE_NARROW,
    PERIOD_BY_MODE,
    UNKNOWN_CELL,
    SlamAggregator,
)
from mapping.slam_local_publisher import SlamLocalPublisher

_BOOT_MS = 1_700_000_000_000


# ─── helpers ───────────────────────────────────────────────────────────

def _grid(shape, occupied_cells=()):
    g = np.full(shape, UNKNOWN_CELL, dtype=np.uint8)
    for r, c in occupied_cells:
        g[r, c] = 255
    return g


def _publisher_emits_delta(
    publisher: SlamLocalPublisher,
    *,
    grid: np.ndarray,
    origin: Pose2D,
    resolution_m: float = 0.10,
    now_ms: int,
):
    """Ingest one tick + force the publisher to emit. Returns the
    SLAMLocalDelta — caller then feeds it to the aggregator."""
    publisher.ingest_local_map(grid, origin, resolution_m, now_ms=now_ms)
    return publisher.due_publish(now_ms=now_ms)


# ─── N=3 followers, disjoint maps ──────────────────────────────────────

def test_three_followers_disjoint_grids_union_bbox():
    """Three followers each cover a different 4 m × 4 m patch; the
    aggregator stitches them into a single canvas spanning the union
    of their bboxes, with contributing_robots = 3.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2,
                          resolution_m=0.10)
    publishers = [
        SlamLocalPublisher(robot_id=f"follower{i}", mode=MODE_DEFAULT)
        for i in (1, 2, 3)
    ]
    # Three 4×4-cell grids at different world origins (0.4 m apart in y).
    grids = [_grid((4, 4), occupied_cells=[(0, 0)]) for _ in range(3)]
    origins = [
        Pose2D(x=0.0, y=0.0, theta_rad=0.0),
        Pose2D(x=0.0, y=0.4, theta_rad=0.0),
        Pose2D(x=0.0, y=0.8, theta_rad=0.0),
    ]
    for pub, g, o in zip(publishers, grids, origins, strict=True):
        delta = _publisher_emits_delta(pub, grid=g, origin=o, now_ms=_BOOT_MS)
        assert delta is not None
        agg.ingest(delta)
    msg = agg.due_aggregate(now_ms=_BOOT_MS)
    assert msg is not None
    assert msg.contributing_robots == 3
    decoded = decode_png_to_grid(msg.occupancy_grid_png)
    # Y-stacked 4-cell grids at 0.4 m apart on a 0.10 m/cell resolution
    # → canvas is 4 wide × 12 tall.
    assert decoded.shape == (12, 4)


# ─── N=3 followers, overlapping conflicting reports ────────────────────

def test_overlapping_followers_preserve_obstacle_via_max():
    """Three followers report the SAME cells at different occupancies
    (some unknown, some free, some occupied). Per-cell max wins:
    occupied beats free beats unknown.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2,
                          resolution_m=0.10)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    pub_c = SlamLocalPublisher(robot_id="f-c")
    # All three at the SAME origin so cells align.
    origin = Pose2D(x=0.0, y=0.0, theta_rad=0.0)
    g_a = np.full((3, 3), 0,   dtype=np.uint8)        # all-free
    g_b = _grid((3, 3))                                # all-unknown
    g_c = _grid((3, 3), occupied_cells=[(1, 1)])      # one obstacle
    for pub, g in ((pub_a, g_a), (pub_b, g_b), (pub_c, g_c)):
        agg.ingest(_publisher_emits_delta(
            pub, grid=g, origin=origin, now_ms=_BOOT_MS))
    msg = agg.due_aggregate(now_ms=_BOOT_MS)
    decoded = decode_png_to_grid(msg.occupancy_grid_png)
    assert decoded.shape == (3, 3)
    # (1,1) cell: max(0, UNKNOWN, 255). Per UNKNOWN-aware merge,
    # UNKNOWN is ignored, so it's max(0, 255) = 255.
    assert decoded[1, 1] == 255
    # Other cells: max(0, UNKNOWN). UNKNOWN is ignored → 0 (free).
    assert decoded[0, 0] == 0


# ─── periodic cadence ──────────────────────────────────────────────────

def test_default_cadence_gates_for_5_seconds():
    """SAN v1.3 §9 — first publish fires immediately; second waits 5 s.
    Validates the v1.3 5 s aggregation window (was 30 s in v1.1).
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    origin = Pose2D()
    for pub in (pub_a, pub_b):
        agg.ingest(_publisher_emits_delta(
            pub, grid=_grid((2, 2)), origin=origin, now_ms=_BOOT_MS))
    assert agg.due_aggregate(now_ms=_BOOT_MS) is not None
    # 4.999 s later — still inside the v1.3 period.
    assert agg.due_aggregate(now_ms=_BOOT_MS + 4_999) is None
    # 5 s — boundary, must fire.
    assert agg.due_aggregate(now_ms=_BOOT_MS + 5_000) is not None


def test_narrow_mode_shortens_cadence_to_2_5s():
    """SAN v1.3 §9 — narrow mode (도심 침투) cadence is 2.5 s
    (was 15 s in v1.1). Mode switch is honored without forcing an
    immediate publish.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    origin = Pose2D()
    for pub in (pub_a, pub_b):
        agg.ingest(_publisher_emits_delta(
            pub, grid=_grid((2, 2)), origin=origin, now_ms=_BOOT_MS))
    agg.due_aggregate(now_ms=_BOOT_MS)
    agg.set_mode(MODE_NARROW)
    # 2.499 s later — still under the 2.5 s narrow cadence.
    assert agg.due_aggregate(now_ms=_BOOT_MS + 2_499) is None
    # 2.5 s — fires.
    assert agg.due_aggregate(now_ms=_BOOT_MS + 2_500) is not None
    assert PERIOD_BY_MODE[MODE_NARROW] == 2.5


# ─── registration-error proxy ──────────────────────────────────────────

def test_two_followers_same_obstacle_global_coord_preserved():
    """Spec line: registration error ≤ 0.5 m. As a pure-compute proxy
    (the real S15-4 needs Gazebo), assert: when two followers report
    the same obstacle at the same world coordinate, the aggregated
    grid still has that exact cell flagged. No shift, no loss.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2,
                          resolution_m=0.10)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    # Both anchor at world origin; both report cell (2, 3) as occupied.
    grid_with_obstacle = _grid((6, 6), occupied_cells=[(2, 3)])
    origin = Pose2D(x=0.0, y=0.0)
    for pub in (pub_a, pub_b):
        agg.ingest(_publisher_emits_delta(
            pub, grid=grid_with_obstacle, origin=origin, now_ms=_BOOT_MS))
    msg = agg.due_aggregate(now_ms=_BOOT_MS)
    decoded = decode_png_to_grid(msg.occupancy_grid_png)
    assert decoded.shape == (6, 6)
    assert decoded[2, 3] == 255, (
        "S15-4 proxy: same-coordinate obstacle should survive the "
        "two-robot fusion without shifting to a neighbour cell")


# ─── min_contributors gate ─────────────────────────────────────────────

def test_aggregator_silent_below_min_contributors():
    """A single follower's report shouldn't trigger an aggregated
    broadcast — that's just an echo, not a fusion. Default
    min_contributors=2 enforces this.
    """
    agg = SlamAggregator(mode=MODE_DEFAULT)  # default min=2
    pub = SlamLocalPublisher(robot_id="lonely")
    agg.ingest(_publisher_emits_delta(
        pub, grid=_grid((2, 2)), origin=Pose2D(), now_ms=_BOOT_MS))
    assert agg.due_aggregate(now_ms=_BOOT_MS) is None


# ─── force-event on formation change ───────────────────────────────────

def test_force_event_publishes_mid_window():
    """Formation-transition event triggers an immediate fresh map even
    inside the current cadence window (e.g. after the leader flips
    from RECON → ASSAULT, the swarm wants a refreshed global map).
    """
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    for pub in (pub_a, pub_b):
        agg.ingest(_publisher_emits_delta(
            pub, grid=_grid((2, 2)), origin=Pose2D(), now_ms=_BOOT_MS))
    agg.due_aggregate(now_ms=_BOOT_MS)
    # 1 s later — normally silent.
    agg.force_event()
    assert agg.due_aggregate(now_ms=_BOOT_MS + 1_000) is not None


# ─── output validation ────────────────────────────────────────────────

def test_published_aggregated_map_validates():
    agg = SlamAggregator(mode=MODE_DEFAULT, min_contributors=2)
    pub_a = SlamLocalPublisher(robot_id="f-a")
    pub_b = SlamLocalPublisher(robot_id="f-b")
    for pub in (pub_a, pub_b):
        agg.ingest(_publisher_emits_delta(
            pub, grid=_grid((4, 4)), origin=Pose2D(), now_ms=_BOOT_MS))
    msg = agg.due_aggregate(now_ms=_BOOT_MS)
    msg.validate()
    assert msg.resolution_m == 0.10
    # PNG round-trip through the codec preserves the encoded grid.
    decoded = decode_png_to_grid(msg.occupancy_grid_png)
    assert decoded.shape == (msg.height_cells, msg.width_cells)


# ─── direct codec sanity ──────────────────────────────────────────────

def test_helper_publisher_emits_valid_delta():
    """Sanity: the helper used throughout this file produces a delta
    that itself validates — catches a bug in the helper before it
    contaminates the scenario assertions."""
    pub = SlamLocalPublisher(robot_id="r")
    delta = _publisher_emits_delta(
        pub, grid=_grid((2, 2)), origin=Pose2D(), now_ms=_BOOT_MS)
    delta.validate()
    # PNG decodes cleanly.
    decoded = decode_png_to_grid(delta.occupancy_grid_delta_png)
    assert decoded.shape == (2, 2)


def test_helper_grid_encode_round_trip():
    """The local-publisher helper builds its PNG via the project's
    encode_grid_to_png; quick round-trip check the encoding is lossless
    for uint8 occupancy grids."""
    src = _grid((5, 5), occupied_cells=[(0, 0), (2, 2), (4, 4)])
    decoded = decode_png_to_grid(encode_grid_to_png(src))
    assert np.array_equal(decoded, src)
