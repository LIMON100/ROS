"""Follower-side SLAM local-delta publisher.

PHASE 4 of the v1.0→v1.1 IDS rollout (SAN-SDD-SWARM-001 v1.1 §9,
SAN-SDD-SUR-001 v1.1 §5).

Replaces the v1.0 1 Hz SLAMDelta firehose with a 30–60 s aggregated
SLAMLocalDelta. The follower's slam_toolbox keeps emitting at 1 Hz
internally; this publisher accumulates those frames across the window,
encodes the result as a PNG, and emits one message per period. Mesh
bandwidth drops by ~100× without losing fidelity, since each frame from
slam_toolbox is itself a running cumulative grid (the latest snapshot
already contains everything known so far).

Dynamic period table (mirrors the Hub-side SlamAggregator):
    wide        60 s   광역 정찰
    default     30 s   일반
    narrow      15 s   도심 침투
    obstacle    10 s   장애물 다발 일시 단축

This module is pure compute. A follower process (or test) calls
`ingest_local_map(grid, origin, resolution_m, now_ms)` on every
slam_toolbox tick and `due_publish(now_ms)` on its own clock; the
publisher returns a SLAMLocalDelta when the period has elapsed, else
None. No queue handles or threads live inside the class.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np

from core.messages import Pose2D, SLAMLocalDelta
from mapping.aggregated_map import encode_grid_to_png

# UNKNOWN cell value in the 8-bit grayscale PNG convention used by
# mapping.aggregated_map: 0 = free, 127 = unknown, 255 = occupied.
UNKNOWN_CELL = 127

# Aggregation modes → period in seconds. Identical to the Hub-side
# SlamAggregator so the two ends agree on cadence when a mode switch
# arrives via a future swarm broadcast.
MODE_WIDE     = "wide"
MODE_DEFAULT  = "default"
MODE_NARROW   = "narrow"
MODE_OBSTACLE = "obstacle"
MODES = (MODE_WIDE, MODE_DEFAULT, MODE_NARROW, MODE_OBSTACLE)

PERIOD_BY_MODE = {
    MODE_WIDE:     30.0,
    MODE_DEFAULT:   5.0,    # SAN v1.3 §9 — was 30 s in v1.1
    MODE_NARROW:    2.5,    # SAN v1.3 §9 — was 15 s in v1.1
    MODE_OBSTACLE:  1.0,    # SAN v1.3 §9 — was 10 s in v1.1
}


@dataclass(frozen=True)
class _Frame:
    """One slam_toolbox ingest pinned to a wall-clock timestamp."""
    grid: np.ndarray
    origin: Pose2D
    resolution_m: float
    ms: int


def _merge_into_canvas(
    canvas: np.ndarray, patch: np.ndarray,
) -> np.ndarray:
    """Per-cell max merge with UNKNOWN-aware semantics.

    Where the canvas is UNKNOWN, take the patch; where the patch is
    UNKNOWN, keep the canvas; where both have observations, take the
    more-occupied value. This is the same rule used by the Hub-side
    SlamAggregator's `merge_deltas`, kept here to avoid coupling the
    follower module to the Hub module.
    """
    if canvas.shape != patch.shape:
        # Different geometries (the local map grew) — caller is
        # responsible for re-anchoring. We just return the patch.
        return patch.copy()
    canvas_unk = (canvas == UNKNOWN_CELL)
    patch_unk  = (patch  == UNKNOWN_CELL)
    max_known  = np.maximum(canvas, patch)
    return np.where(canvas_unk, patch,
           np.where(patch_unk,  canvas, max_known)).astype(np.uint8)


class SlamLocalPublisher:
    """Follower-side accumulator + PNG-encoded SLAMLocalDelta emitter.

    Threading: not internally synchronised. The follower wraps it in a
    lock when slam_toolbox callbacks race the periodic publish thread.
    """

    def __init__(
        self,
        robot_id: str,
        mode: str = MODE_DEFAULT,
        period_sec: Optional[float] = None,
    ):
        if not robot_id:
            raise ValueError("robot_id must be non-empty")
        if mode not in MODES:
            raise ValueError(f"unknown aggregation mode: {mode!r}")
        self._robot_id = str(robot_id)
        self._mode = mode
        self._period_sec = (
            float(period_sec) if period_sec is not None
            else PERIOD_BY_MODE[mode])
        # Per-window accumulation state.
        self._canvas: Optional[np.ndarray] = None
        self._latest_origin: Optional[Pose2D] = None
        self._latest_resolution: Optional[float] = None
        # Window timing.
        self._coverage_start_ms: Optional[int] = None
        self._last_publish_ms: Optional[int] = None
        self._sequence: int = 0
        self._stats = {"ingests": 0, "publishes": 0, "geometry_resets": 0}

    # ─── configuration ─────────────────────────────────────────────────

    @property
    def robot_id(self) -> str:
        return self._robot_id

    @property
    def mode(self) -> str:
        return self._mode

    @property
    def period_sec(self) -> float:
        return self._period_sec

    @property
    def stats(self) -> dict:
        return dict(self._stats)

    def set_mode(self, mode: str) -> None:
        """Switch aggregation mode → new period. Identical to the
        SlamAggregator hook so a swarm-wide mode broadcast can drive
        both ends in lockstep. Mode change does NOT force an immediate
        publish; the new period takes effect on the next `due_publish`.
        """
        if mode not in MODES:
            raise ValueError(f"unknown aggregation mode: {mode!r}")
        if mode == self._mode:
            return
        self._mode = mode
        self._period_sec = PERIOD_BY_MODE[mode]

    def set_period_sec(self, period_sec: float) -> None:
        """Override the period in seconds without changing the named mode.

        Used by tests and by operators who want a non-standard cadence
        (e.g. 20 s for a one-off mission). Must be positive.
        """
        if period_sec <= 0:
            raise ValueError(
                f"period_sec must be positive: {period_sec}")
        self._period_sec = float(period_sec)

    # ─── ingest ────────────────────────────────────────────────────────

    def ingest_local_map(
        self,
        grid: np.ndarray,
        origin: Pose2D,
        resolution_m: float,
        now_ms: int,
    ) -> None:
        """Fold a slam_toolbox tick into the current window.

        Subsequent ingests with the same geometry (shape + origin +
        resolution) merge via per-cell max; a geometry change resets
        the canvas to the new patch (the local map grew). The
        coverage_start_ms field locks onto the first ingest of the
        window.
        """
        if grid.dtype != np.uint8:
            grid = grid.astype(np.uint8)
        self._stats["ingests"] += 1
        if self._coverage_start_ms is None:
            self._coverage_start_ms = int(now_ms)
        same_geom = (
            self._canvas is not None
            and self._canvas.shape == grid.shape
            and self._latest_origin == origin
            and self._latest_resolution == float(resolution_m))
        if not same_geom:
            if self._canvas is not None:
                self._stats["geometry_resets"] += 1
            self._canvas = grid.copy()
        else:
            self._canvas = _merge_into_canvas(self._canvas, grid)
        self._latest_origin = origin
        self._latest_resolution = float(resolution_m)

    # ─── output ────────────────────────────────────────────────────────

    def due_publish(self, now_ms: int) -> Optional[SLAMLocalDelta]:
        """Emit a SLAMLocalDelta when the configured period has elapsed.

        Returns None when:
          - No ingest has happened yet (nothing to encode)
          - The period hasn't elapsed since the last publish

        On publish, the window resets: a fresh ingest starts the next
        coverage window. The first call after the first ingest always
        fires (boot-time pre-roll), so a slow follower's first map
        reaches the Hub at startup rather than waiting an extra cycle.
        """
        if self._canvas is None:
            return None
        if self._last_publish_ms is not None:
            elapsed_ms = int(now_ms) - self._last_publish_ms
            if elapsed_ms < int(self._period_sec * 1000):
                return None
        png = encode_grid_to_png(self._canvas)
        self._sequence += 1
        msg = SLAMLocalDelta(
            sequence=self._sequence,
            robot_id=self._robot_id,
            occupancy_grid_delta_png=png,
            origin=self._latest_origin or Pose2D(),
            resolution_m=float(self._latest_resolution or 0.10),
            coverage_start_ms=int(self._coverage_start_ms or now_ms),
            coverage_end_ms=int(now_ms),
            timestamp_ms=int(now_ms),
        )
        msg.validate()
        self._stats["publishes"] += 1
        # Reset window.
        self._last_publish_ms = int(now_ms)
        self._coverage_start_ms = None
        self._canvas = None
        # We deliberately keep _latest_origin / _latest_resolution so a
        # follow-up ingest with the same geometry (very likely on the
        # next slam_toolbox tick) doesn't trigger a spurious reset.
        return msg


__all__ = (
    "MODES",
    "MODE_DEFAULT",
    "MODE_NARROW",
    "MODE_OBSTACLE",
    "MODE_WIDE",
    "PERIOD_BY_MODE",
    "SlamLocalPublisher",
    "UNKNOWN_CELL",
)
