"""Hub UGV aggregated-map broadcast helpers.

Topic: /hub/slam/aggregated_map  (QoS P2 RELIABLE TRANSIENT_LOCAL)
Cadence: SAN v1.3 = **5 s** (was 30–60 s in v1.1; ADR-002 covers the
v1.1 rationale, v1.3 §9 reverses it now that the Hub UGV runs on dual
SBC compute, see §3.4).

Encodes the full fused occupancy grid as a PNG byte string so it can
ride a single message (instead of the chunked path that lives in
swarm_bridge → shared_map_in). The Hub UGV SBC#1 owns this dispatcher;
followers run `decode_png_to_grid` on the receive side.

PIL/Pillow is the only PNG codec on offer in pure-Python; we import it
lazily so the module loads in environments without Pillow (dispatcher
construction still works; encoding then raises a clear error).
"""
from __future__ import annotations

import io
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from core.messages import AggregatedMap, Pose2D

# SAN v1.3 §9 cadence — Hub UGV dual-SBC has enough compute to fuse
# every 5 s. The legacy v1.1 floor of 30 s lives on as LEGACY_DEFAULT_*
# so existing callers (tests, dashboards) can opt back into the slower
# pace if they need to.
DEFAULT_PERIOD_S = 5.0
MIN_PERIOD_S = 1.0          # below 1 s, the PNG round-trip dominates
MAX_PERIOD_S = 60.0
LEGACY_DEFAULT_PERIOD_S = 30.0    # v1.1 baseline (ADR-002, pre-v1.3)


# ─── PNG codec ──────────────────────────────────────────────────────────

def _require_pillow():
    try:
        from PIL import Image  # noqa: F401
    except ImportError as e:    # pragma: no cover - exercised via tests
        raise RuntimeError(
            "Pillow is required for AggregatedMap PNG encoding; "
            "install with `pip install Pillow`") from e


def encode_grid_to_png(grid: np.ndarray) -> bytes:
    """Encode a 2D occupancy grid as an 8-bit grayscale PNG.

    Accepts either an int8/uint8 grid (already 0..255) or a float grid in
    [-1, 1] where -1=unknown, 0=free, 1=occupied — float inputs are
    mapped: -1 → 127 (mid-gray, unknown), 0 → 0, 1 → 255.
    """
    _require_pillow()
    from PIL import Image
    if grid.ndim != 2:
        raise ValueError(f"grid must be 2D, got shape {grid.shape}")
    if grid.dtype in (np.float32, np.float64):
        # -1 (unknown) → 127, 0 (free) → 0, 1 (occupied) → 255
        unknown = (grid < 0).astype(np.uint8) * 127
        occ = np.clip(grid, 0.0, 1.0)
        rendered = np.where(grid < 0, unknown,
                            (occ * 255.0).astype(np.uint8))
        arr = rendered.astype(np.uint8)
    else:
        arr = grid.astype(np.uint8)
    buf = io.BytesIO()
    Image.fromarray(arr, mode="L").save(buf, format="PNG", optimize=False)
    return buf.getvalue()


def decode_png_to_grid(png_bytes: bytes) -> np.ndarray:
    """Decode PNG bytes back to a (H, W) uint8 occupancy grid."""
    _require_pillow()
    from PIL import Image
    img = Image.open(io.BytesIO(png_bytes))
    if img.mode != "L":
        img = img.convert("L")
    return np.asarray(img, dtype=np.uint8)


# ─── Dispatcher ─────────────────────────────────────────────────────────

@dataclass
class AggregatedMapInput:
    """Inputs the dispatcher needs to build one AggregatedMap.

    `grid` is the fused occupancy grid (float in [-1, 1] or uint8 0..255).
    `origin` is the bottom-left world coordinate + grid rotation; for an
    axis-aligned grid pass theta_rad=0.
    """
    grid: np.ndarray
    origin: Pose2D
    resolution_m: float
    contributing_robots: int


class AggregatedMapDispatcher:
    """Stateful publisher helper.

    Holds the periodic timer + sequence counter. Caller invokes
    `due_message(now_ms, inputs)` once per tick; the dispatcher returns
    an AggregatedMap when the period has elapsed, else None.

    `period_s` is clamped to [MIN_PERIOD_S, MAX_PERIOD_S] = [1, 60] s
    per v1.3. Sub-second periods are rejected because the PNG encode
    round-trip alone runs ~50–100 ms on the Hub SBC and we need
    breathing room for the per-tick fuse.
    """

    def __init__(self, period_s: float = DEFAULT_PERIOD_S):
        if period_s < MIN_PERIOD_S:
            period_s = MIN_PERIOD_S
        if period_s > MAX_PERIOD_S:
            period_s = MAX_PERIOD_S
        self.period_s = float(period_s)
        self._sequence: int = 0
        self._last_publish_ms: Optional[int] = None

    def due_message(
        self,
        now_ms: int,
        inputs: Optional[AggregatedMapInput],
    ) -> Optional[AggregatedMap]:
        """Return one AggregatedMap if the period elapsed AND inputs are
        present; else None. The first call publishes immediately."""
        if inputs is None:
            return None
        if self._last_publish_ms is not None:
            elapsed_ms = now_ms - self._last_publish_ms
            if elapsed_ms < int(self.period_s * 1000):
                return None
        return self._build(now_ms, inputs)

    def event_message(
        self,
        now_ms: int,
        inputs: AggregatedMapInput,
    ) -> AggregatedMap:
        """Force-publish (e.g. on first hub election). Resets the timer."""
        return self._build(now_ms, inputs)

    def _build(
        self,
        now_ms: int,
        inputs: AggregatedMapInput,
    ) -> AggregatedMap:
        h, w = inputs.grid.shape
        png = encode_grid_to_png(inputs.grid)
        self._sequence += 1
        msg = AggregatedMap(
            sequence=self._sequence,
            occupancy_grid_png=png,
            origin=inputs.origin,
            resolution_m=float(inputs.resolution_m),
            width_cells=int(w),
            height_cells=int(h),
            contributing_robots=int(inputs.contributing_robots),
            timestamp_ms=now_ms,
        )
        msg.validate()
        self._last_publish_ms = now_ms
        return msg


# ─── Consumer helper ────────────────────────────────────────────────────

def decode_aggregated_map(msg: AggregatedMap) -> Tuple[np.ndarray, Pose2D, float]:
    """Decode an AggregatedMap into (grid, origin, resolution_m).

    Verifies the decoded grid shape matches width/height_cells; raises
    ValueError on mismatch.
    """
    grid = decode_png_to_grid(msg.occupancy_grid_png)
    if (msg.height_cells, msg.width_cells) != grid.shape:
        raise ValueError(
            f"AggregatedMap dimension mismatch: "
            f"header {msg.height_cells}x{msg.width_cells}, "
            f"PNG {grid.shape[0]}x{grid.shape[1]}")
    return grid, msg.origin, msg.resolution_m


__all__ = (
    "DEFAULT_PERIOD_S",
    "LEGACY_DEFAULT_PERIOD_S",
    "MAX_PERIOD_S",
    "MIN_PERIOD_S",
    "AggregatedMapDispatcher",
    "AggregatedMapInput",
    "decode_aggregated_map",
    "decode_png_to_grid",
    "encode_grid_to_png",
)
