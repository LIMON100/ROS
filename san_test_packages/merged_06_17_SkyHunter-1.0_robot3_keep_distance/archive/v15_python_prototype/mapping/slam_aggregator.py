"""Hub UGV SBC#1 SLAM aggregator.

Consumes follower `SLAMLocalDelta` messages (per /{robot_id}/slam/local_delta)
and produces a single `AggregatedMap` on /hub/slam/aggregated_map at a
mode-dependent cadence. Pure compute: callers (a thread in the Hub's
SLAM process) feed deltas in and pull aggregates out — this module owns
no queue handles.

Dynamic period table (SAN v1.3 §9 — Hub UGV dual-SBC compute):
    wide        30 s    광역 정찰
    default      5 s    일반 (v1.3 baseline; was 30 s in v1.1)
    narrow     2.5 s    도심 침투 (was 15 s in v1.1)
    obstacle     1 s    장애물 다발 일시 단축 (was 10 s in v1.1)
    + force_event() schedules an immediate publish on the next due tick
      (대형 전환 / formation transition).

The bandwidth budget remains under the v1.1 mesh saturation cap
(see tests/test_slam_local_publisher.test_bandwidth_*); the 6× faster
default trades CPU for freshness on the Hub side, which the dual-SBC
upgrade supplies (SDD §3.4).

Multi-robot fusion v1: per-cell *max* over the contributing deltas
inside a bounding-box canvas. Pose-graph optimization is a stubbed
identity pass — the upgrade path is a g2o/GTSAM solver but the data
flow + API don't change.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, Optional

import numpy as np

from core.messages import AggregatedMap, Pose2D, SLAMLocalDelta
from mapping.aggregated_map import (
    AggregatedMapDispatcher,
    AggregatedMapInput,
    decode_png_to_grid,
)

# Cell value for "unknown" in the 8-bit grayscale convention used by
# encode_grid_to_png — middle-gray means "no robot saw this cell."
UNKNOWN_CELL = 127

# Aggregation modes → period in seconds. Mirrors SUR §4.2 table.
MODE_WIDE     = "wide"
MODE_DEFAULT  = "default"
MODE_NARROW   = "narrow"
MODE_OBSTACLE = "obstacle"
MODES = (MODE_WIDE, MODE_DEFAULT, MODE_NARROW, MODE_OBSTACLE)

PERIOD_BY_MODE: Dict[str, float] = {
    MODE_WIDE:     30.0,
    MODE_DEFAULT:   5.0,
    MODE_NARROW:    2.5,
    MODE_OBSTACLE:  1.0,
}

# Minimum contributors before we'll publish — a "single-follower"
# aggregated map is just that follower's delta and carries no fusion
# value, so the Hub stays quiet until at least two robots reported.
DEFAULT_MIN_CONTRIBUTORS = 2

# Allow short modes to bypass the AggregatedMapDispatcher's [30, 60]
# clamp without breaking back-compat for callers that use the dispatcher
# directly. The aggregator gates its own period internally.
_DISPATCH_PERIOD_FLOOR_S = 1.0


@dataclass(frozen=True)
class _DecodedDelta:
    """Decoded form of a cached SLAMLocalDelta — kept frozen for safety."""
    robot_id: str
    grid: np.ndarray            # (H, W) uint8
    origin: Pose2D
    resolution_m: float
    timestamp_ms: int


def _decode(delta: SLAMLocalDelta) -> _DecodedDelta:
    grid = decode_png_to_grid(delta.occupancy_grid_delta_png)
    return _DecodedDelta(
        robot_id=delta.robot_id,
        grid=grid,
        origin=delta.origin,
        resolution_m=float(delta.resolution_m),
        timestamp_ms=int(delta.timestamp_ms),
    )


def merge_deltas(
    deltas: Iterable[_DecodedDelta],
    resolution_m: float,
) -> Optional[AggregatedMapInput]:
    """Stitch decoded deltas onto a single canvas covering all bboxes.

    Returns None when `deltas` is empty. Per-cell rule on overlap: max
    (most-occupied wins), which favours preserving obstacle reports
    over free/unknown reports from a different robot.
    """
    decoded = [d for d in deltas if d.grid.size > 0]
    if not decoded:
        return None
    min_x = min(d.origin.x for d in decoded)
    min_y = min(d.origin.y for d in decoded)
    max_x = max(d.origin.x + d.grid.shape[1] * resolution_m for d in decoded)
    max_y = max(d.origin.y + d.grid.shape[0] * resolution_m for d in decoded)
    canvas_w = int(round((max_x - min_x) / resolution_m))
    canvas_h = int(round((max_y - min_y) / resolution_m))
    if canvas_w <= 0 or canvas_h <= 0:
        return None
    canvas = np.full((canvas_h, canvas_w), UNKNOWN_CELL, dtype=np.uint8)
    for d in decoded:
        ox = int(round((d.origin.x - min_x) / resolution_m))
        oy = int(round((d.origin.y - min_y) / resolution_m))
        h, w = d.grid.shape
        # Defensive clamp: a misaligned origin shouldn't crash the merge.
        ox = max(ox, 0)
        oy = max(oy, 0)
        h = min(h, canvas_h - oy)
        w = min(w, canvas_w - ox)
        if h <= 0 or w <= 0:
            continue
        view  = canvas[oy:oy + h, ox:ox + w]
        patch = d.grid[:h, :w]
        # Merge rule: UNKNOWN_CELL (127) means "no observation from this
        # robot," so it must NOT override a real reading from another.
        # Both-known cells take per-pixel max (most-occupied wins).
        view_unk  = (view == UNKNOWN_CELL)
        patch_unk = (patch == UNKNOWN_CELL)
        max_known = np.maximum(view, patch)
        # canvas ← patch where view was unknown; canvas keeps view where
        # patch is unknown; otherwise canvas ← max.
        merged = np.where(view_unk, patch,
                 np.where(patch_unk, view, max_known))
        canvas[oy:oy + h, ox:ox + w] = merged
    return AggregatedMapInput(
        grid=canvas,
        origin=Pose2D(x=float(min_x), y=float(min_y), theta_rad=0.0),
        resolution_m=float(resolution_m),
        contributing_robots=len(decoded),
    )


def optimize_pose_graph(canvas: np.ndarray) -> np.ndarray:
    """v1 stub — pass-through.

    The PHASE 3 design calls for a g2o/GTSAM pose-graph optimisation
    pass over the merged grid (loop closures, drift correction). That
    requires a per-delta pose-graph payload the SLAMLocalDelta message
    doesn't yet carry; until it does, this is intentionally a no-op so
    the data path is wired and easy to swap.
    """
    return canvas


class SlamAggregator:
    """Hub UGV SBC#1 multi-robot SLAM fuser.

    Threading: not internally synchronised. Wrap with a lock if a
    consumer thread calls `due_aggregate` while a publisher thread
    calls `ingest`.
    """

    def __init__(
        self,
        mode: str = MODE_DEFAULT,
        min_contributors: int = DEFAULT_MIN_CONTRIBUTORS,
        resolution_m: float = 0.10,
    ):
        if mode not in MODES:
            raise ValueError(f"unknown aggregation mode: {mode!r}")
        if min_contributors < 1:
            raise ValueError(
                f"min_contributors must be >= 1: {min_contributors}")
        self._mode = mode
        self._period_s = PERIOD_BY_MODE[mode]
        self._min_contributors = int(min_contributors)
        self._resolution_m = float(resolution_m)
        self._deltas: Dict[str, SLAMLocalDelta] = {}
        self._force_next: bool = False
        self._last_publish_ms: Optional[int] = None
        # The dispatcher carries the sequence + PNG-encode step. We
        # construct it with the dispatcher's own period floor; our
        # `due_aggregate` does the real cadence gating so the dispatcher
        # never short-circuits us with its built-in [30, 60] clamp.
        self._dispatcher = AggregatedMapDispatcher(
            period_s=_DISPATCH_PERIOD_FLOOR_S)

    # ─── configuration ─────────────────────────────────────────────────

    @property
    def mode(self) -> str:
        return self._mode

    @property
    def period_s(self) -> float:
        return self._period_s

    @property
    def contributors(self) -> int:
        return len(self._deltas)

    def set_mode(self, mode: str) -> None:
        """Switch operating mode → new aggregation period.

        Mode change alone does NOT force an immediate publish — that's
        `force_event`. The new period applies on the next due-check.
        """
        if mode not in MODES:
            raise ValueError(f"unknown aggregation mode: {mode!r}")
        if mode == self._mode:
            return
        self._mode = mode
        self._period_s = PERIOD_BY_MODE[mode]

    def force_event(self) -> None:
        """Schedule an immediate aggregate on the next `due_aggregate`.

        Used for formation transitions and other "we need a fresh map
        right now" events. Auto-clears after the next publish.
        """
        self._force_next = True

    # ─── ingest ────────────────────────────────────────────────────────

    def ingest(self, delta: SLAMLocalDelta) -> None:
        """Cache the most recent delta from a follower.

        Replaces any prior delta from the same robot_id — the Hub only
        ever needs the latest snapshot, since each follower's delta
        already represents an aggregated window.
        """
        delta.validate()
        self._deltas[delta.robot_id] = delta

    def forget(self, robot_id: str) -> bool:
        """Drop a robot's cached delta (e.g. follower lost > timeout).

        Returns True when something was removed. The caller is
        responsible for the staleness policy; this module never expires
        on its own so a test can drive `now_ms` deterministically.
        """
        return self._deltas.pop(robot_id, None) is not None

    # ─── output ────────────────────────────────────────────────────────

    def due_aggregate(self, now_ms: int) -> Optional[AggregatedMap]:
        """Produce an AggregatedMap when the period has elapsed.

        Returns None when:
          - fewer than `min_contributors` deltas cached
          - period not yet elapsed AND no force-event pending
          - the merge produced no grid (all deltas were degenerate)
        """
        if len(self._deltas) < self._min_contributors:
            return None
        if not self._force_next and self._last_publish_ms is not None:
            elapsed_ms = now_ms - self._last_publish_ms
            if elapsed_ms < int(self._period_s * 1000):
                return None
        decoded = [_decode(d) for d in self._deltas.values()]
        merged = merge_deltas(decoded, resolution_m=self._resolution_m)
        if merged is None:
            return None
        # Pose-graph optimization (v1: pass-through stub).
        optimized = optimize_pose_graph(merged.grid)
        merged = AggregatedMapInput(
            grid=optimized,
            origin=merged.origin,
            resolution_m=merged.resolution_m,
            contributing_robots=merged.contributing_robots,
        )
        msg = self._dispatcher.event_message(now_ms, merged)
        self._last_publish_ms = now_ms
        self._force_next = False
        return msg


__all__ = (
    "DEFAULT_MIN_CONTRIBUTORS",
    "MODES",
    "MODE_DEFAULT",
    "MODE_NARROW",
    "MODE_OBSTACLE",
    "MODE_WIDE",
    "PERIOD_BY_MODE",
    "SlamAggregator",
    "UNKNOWN_CELL",
    "merge_deltas",
    "optimize_pose_graph",
)
