"""Cost map compositor — drives the 4-layer local cost map (SAN v1.3 §6.4).

Layer order (worst-cost-wins on overlap):
  L1  static          OsmStaticLayer + SlamPersistentLayer (PHASE 0 already
                      provides them — we sample at the local window only)
  L2  obstacle        ObstacleLayer  — 235 mm lethal
  L3  traversability  TraversabilityLayer — 30° slope, 220 mm ditch lethal
  L4  inflation       InflationLayer — 1.0 m halo around lethal cells

The compositor produces a (H, W) uint8 master grid and a CostMapUpdate
message ready to publish. PNG encoding for cross-process transport is
optional and gated on Pillow availability.
"""
from __future__ import annotations

import io
import time
from dataclasses import dataclass
from typing import Iterable, Optional, Tuple

import numpy as np

from core.messages import (
    COST_FREE,
    COST_LETHAL,
    COST_UNKNOWN,
    COST_WARN,
    CostMapUpdate,
    Header,
)

from .inflation_layer import InflationLayer, InflationParams
from .obstacle_layer import ObstacleLayer, ObstacleThresholds
from .traversability_layer import (
    TraversabilityLayer,
    TraversabilityThresholds,
)

try:
    from PIL import Image  # type: ignore
    _HAS_PIL = True
except ImportError:        # pragma: no cover — same fallback as aggregated_map
    _HAS_PIL = False


@dataclass
class CostMapConfig:
    """All knobs the compositor cares about. Populate from a Config via
    `CostMapConfig.from_cfg(cfg)`.
    """
    size_m: float = 14.0
    resolution_m: float = 0.05
    publish_rate_hz: float = 1.0
    obstacle_lethal_mm: float = 235.0
    obstacle_warn_mm: float = 200.0
    obstacle_min_points: int = 3
    slope_lethal_deg: float = 30.0
    slope_warn_deg: float = 25.0
    ditch_lethal_mm: float = 220.0
    ditch_warn_mm: float = 150.0
    inflation_radius_m: float = 1.0
    inflation_decay_rate: float = 5.0

    @classmethod
    def from_cfg(cls, cfg) -> "CostMapConfig":
        c = cfg.section("cost_map") if hasattr(cfg, "section") else {}
        # Tolerate either a Config object (.section()) or a plain dict.
        if not c and isinstance(cfg, dict):
            c = cfg.get("cost_map", {}) or {}
        return cls(
            size_m=float(c.get("size_m", cls.size_m)),
            resolution_m=float(c.get("resolution_m", cls.resolution_m)),
            publish_rate_hz=float(c.get("publish_rate_hz",
                                        cls.publish_rate_hz)),
            obstacle_lethal_mm=float(c.get("obstacle_lethal_mm",
                                           cls.obstacle_lethal_mm)),
            obstacle_warn_mm=float(c.get("obstacle_warn_mm",
                                         cls.obstacle_warn_mm)),
            obstacle_min_points=int(c.get("obstacle_min_points",
                                          cls.obstacle_min_points)),
            slope_lethal_deg=float(c.get("slope_lethal_deg",
                                         cls.slope_lethal_deg)),
            slope_warn_deg=float(c.get("slope_warn_deg",
                                       cls.slope_warn_deg)),
            ditch_lethal_mm=float(c.get("ditch_lethal_mm",
                                        cls.ditch_lethal_mm)),
            ditch_warn_mm=float(c.get("ditch_warn_mm", cls.ditch_warn_mm)),
            inflation_radius_m=float(c.get("inflation_radius_m",
                                           cls.inflation_radius_m)),
            inflation_decay_rate=float(c.get("inflation_decay_rate",
                                             cls.inflation_decay_rate)),
        )

    @property
    def grid_cells(self) -> int:
        return int(round(self.size_m / self.resolution_m))


class CostMap:
    """4-layer compositor. Stateless across `compose()` calls — the
    layers carry their own per-tick grids and we just OR them together.
    """

    def __init__(self, cfg: Optional[CostMapConfig] = None):
        self.cfg = cfg or CostMapConfig()
        cfg = self.cfg
        self.obstacle = ObstacleLayer(
            size_m=cfg.size_m, resolution_m=cfg.resolution_m,
            thresholds=ObstacleThresholds(
                lethal_mm=cfg.obstacle_lethal_mm,
                warn_mm=cfg.obstacle_warn_mm,
                min_points=cfg.obstacle_min_points))
        self.traversability = TraversabilityLayer(
            size_m=cfg.size_m, resolution_m=cfg.resolution_m,
            thresholds=TraversabilityThresholds(
                slope_lethal_deg=cfg.slope_lethal_deg,
                slope_warn_deg=cfg.slope_warn_deg,
                ditch_lethal_mm=cfg.ditch_lethal_mm,
                ditch_warn_mm=cfg.ditch_warn_mm))
        self.inflation = InflationLayer(
            size_m=cfg.size_m, resolution_m=cfg.resolution_m,
            params=InflationParams(
                radius_m=cfg.inflation_radius_m,
                decay_rate=cfg.inflation_decay_rate))
        n = cfg.grid_cells
        self.master = np.full((n, n), COST_FREE, dtype=np.uint8)

    @property
    def shape(self) -> Tuple[int, int]:
        return self.master.shape

    @property
    def forward_coverage_m(self) -> float:
        """Sensor-forward coverage (m). Robot is at the grid center."""
        return self.cfg.size_m / 2.0

    def compose(self,
                points_xyz: np.ndarray,
                static_layer_window: Optional[np.ndarray] = None,
                t_input_mono: Optional[float] = None
                ) -> Tuple[np.ndarray, float]:
        """Run all 4 layers on a fresh LiDAR scan + an optional static
        window, return (master_grid, producer_latency_s).

        `static_layer_window` is a (H, W) uint8 grid the caller sampled
        from OsmStaticLayer / SlamPersistentLayer at the robot's pose.
        Pass None to skip the static layer (still valid — useful for
        bench tests where no map is loaded).

        `t_input_mono` is `time.monotonic()` when the LiDAR scan was
        captured. If None, we use compose-entry time so the latency
        sample is bounded but pessimistic.
        """
        t_start = t_input_mono if t_input_mono is not None else time.monotonic()

        n = self.cfg.grid_cells
        m = np.full((n, n), COST_FREE, dtype=np.uint8)

        # L1 static — already a uint8 grid we just merge in.
        if static_layer_window is not None:
            if static_layer_window.shape != m.shape:
                raise ValueError(
                    f"static layer window shape "
                    f"{static_layer_window.shape} != master {m.shape}")
            np.maximum(m, static_layer_window.astype(np.uint8), out=m)

        # L2 obstacle.
        obs = self.obstacle.update(points_xyz)
        np.maximum(m, obs, out=m)

        # L3 traversability.
        trav = self.traversability.update(points_xyz)
        np.maximum(m, trav, out=m)

        # L4 inflation — driven by the LETHAL cells in the partial
        # master after L1-L3.
        lethal_mask = m == COST_LETHAL
        infl = self.inflation.update(lethal_mask)
        # Inflation never downgrades a lethal cell or overrides higher
        # costs — `maximum` keeps the worst.
        np.maximum(m, infl, out=m)

        self.master = m
        return m, max(0.0, time.monotonic() - t_start)

    # ── Message envelope ──────────────────────────────────────────

    def to_message(self,
                   producer_latency_s: float,
                   origin_xy: Tuple[float, float] = (0.0, 0.0),
                   encoding: str = "raw") -> CostMapUpdate:
        """Wrap the current master grid as a CostMapUpdate.

        `origin_xy` is the world coord of the grid's (0, 0) cell. Pass
        the robot's pose minus (size_m/2, size_m/2) for a sane default.
        """
        n_lethal = int(np.count_nonzero(self.master == COST_LETHAL))
        n_warn = int(np.count_nonzero(self.master == COST_WARN))
        n_unknown = int(np.count_nonzero(self.master == COST_UNKNOWN))
        n_free = int(self.master.size - n_lethal - n_warn - n_unknown)

        if encoding == "png":
            payload = encode_master_png(self.master)
        elif encoding == "raw":
            payload = self.master.tobytes()
        else:
            raise ValueError(f"unknown encoding: {encoding!r}")

        h, w = self.master.shape
        return CostMapUpdate(
            header=Header(stamp=time.monotonic()),
            width=int(w),
            height=int(h),
            resolution_m=float(self.cfg.resolution_m),
            origin_xy=tuple(float(v) for v in origin_xy),
            master_payload=payload,
            encoding=encoding,
            n_lethal=n_lethal,
            n_warn=n_warn,
            n_free=n_free,
            n_unknown=n_unknown,
            producer_latency_s=float(producer_latency_s),
        )


# ── PNG codec (cross-process payload) ──────────────────────────────
def encode_master_png(grid: np.ndarray) -> bytes:
    """uint8 grid → PNG bytes (greyscale, deflate). Returns raw bytes
    when Pillow is not installed so we still ship a usable payload.
    """
    if grid.dtype != np.uint8:
        raise ValueError("master grid must be uint8")
    if not _HAS_PIL:
        return grid.tobytes()
    buf = io.BytesIO()
    Image.fromarray(grid, mode="L").save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def decode_master(payload: bytes,
                  width: int,
                  height: int,
                  encoding: str) -> np.ndarray:
    """Inverse of encode_master_png() / raw bytes. Returns uint8 grid."""
    if not payload:
        return np.zeros((height, width), dtype=np.uint8)
    if encoding == "raw":
        arr = np.frombuffer(payload, dtype=np.uint8)
        return arr.reshape((height, width))
    if encoding == "png":
        if not _HAS_PIL:
            raise RuntimeError(
                "PNG decode requires Pillow; install with `pip install Pillow`")
        with Image.open(io.BytesIO(payload)) as im:
            arr = np.asarray(im.convert("L"), dtype=np.uint8)
        if arr.shape != (height, width):
            raise ValueError(
                f"PNG shape mismatch: got {arr.shape}, expected "
                f"({height}, {width})")
        return arr
    raise ValueError(f"unknown encoding: {encoding!r}")


# ── Convenience: build a master grid directly without instantiating
# the layers (used by tests + by callers that only want one shot).
def compose_once(points_xyz: np.ndarray,
                 cfg: Optional[CostMapConfig] = None,
                 static_layer_window: Optional[np.ndarray] = None
                 ) -> np.ndarray:
    cm = CostMap(cfg or CostMapConfig())
    grid, _ = cm.compose(points_xyz, static_layer_window)
    return grid


__all__ = [
    "CostMap",
    "CostMapConfig",
    "compose_once",
    "decode_master",
    "encode_master_png",
]


def _validate_layer_ids(layers: Iterable[int]) -> None:
    """Defensive check used by the layered debug overlay (tests only)."""
    expected = (0, 1, 2, 3)
    if tuple(sorted(set(layers))) != expected:
        raise ValueError(f"layers must be {expected}, got {tuple(layers)}")
