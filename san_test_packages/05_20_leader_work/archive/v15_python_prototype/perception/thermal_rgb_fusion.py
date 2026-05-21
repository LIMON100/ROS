"""
Thermal + RGB fusion for anomaly enrichment.

Two cameras with different timing & geometry:

  ┌─ IMX678 (Sony 4K RGB) ──┐      Resolution:  3840×2160 @ 15 Hz
  │                         │      Lens:        wide angle, fov ≈ 90°
  └─────────────────────────┘      Output:      H.265 NAL units in SHM

  ┌─ Thermal (FLIR Boson 640) ─┐   Resolution:  640×512 @ 9 Hz
  │                            │   Lens:        50° fov typical
  └────────────────────────────┘   Output:      mono16 in SHM

To enrich a detected RGB anomaly (e.g. a person without PPE) with thermal
context (body-temp range), we need to:

  1. Time-sync — find the thermal frame whose timestamp is closest to the
     RGB frame, within a tolerance (default 100 ms = ½ thermal period).
  2. Project — compute where in the thermal frame the RGB bounding box
     lands. Both lenses share roughly co-located optical centers (mounted
     stereo-baseline ≤ 5 cm), so for distant subjects (>3 m) we can use
     a calibrated rectified homography. For closer subjects, parallax
     correction would be needed (out of scope for PoC).
  3. Aggregate — read the thermal pixels inside the projected box, return
     min/mean/max temperatures.

This module is deliberately decoupled from RKNN inference (PerceptionProcess
calls FuseTracker.enrich_detection()). Cameras feed timestamps + frame
references via add_rgb_frame() / add_thermal_frame().
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from typing import Deque, Optional, Tuple

import numpy as np


# ────────── Time-sync buffer ──────────
@dataclass
class _FrameStub:
    """Lightweight frame metadata used by the syncer.

    `data` is opaque — the syncer doesn't decode anything.
    """
    stamp: float
    data: object


class TimeSyncBuffer:
    """Bounded-size buffer of (stamp, data) pairs with nearest-neighbor lookup.

    Keeps newest entries; oldest get evicted past max_size. Lookups are O(N)
    but N is tiny (typically ≤ 30 entries — 3 s at thermal rate).
    """
    def __init__(self, max_size: int = 30):
        self._buf: Deque[_FrameStub] = deque(maxlen=max_size)

    def add(self, stamp: float, data: object) -> None:
        # Maintain monotonic order; if a stamp arrives out of order, ignore
        # (we do not splice — this is a real-time stream, not an archive).
        if self._buf and stamp < self._buf[-1].stamp:
            return
        self._buf.append(_FrameStub(stamp=stamp, data=data))

    def nearest(self, stamp: float, max_dt_s: float = 0.10
                ) -> Optional[Tuple[float, object]]:
        """Return (stamp, data) of frame closest to `stamp` within tolerance."""
        if not self._buf:
            return None
        # Closest in absolute time difference
        best = min(self._buf, key=lambda f: abs(f.stamp - stamp))
        if abs(best.stamp - stamp) > max_dt_s:
            return None
        return best.stamp, best.data

    def __len__(self) -> int:
        return len(self._buf)


# ────────── Calibration / projection ──────────
@dataclass
class CameraCalib:
    """Pinhole intrinsics for a single camera.

    K = [[fx, 0, cx], [0, fy, cy], [0, 0, 1]]
    """
    width: int
    height: int
    fx: float
    fy: float
    cx: float
    cy: float

    @property
    def K(self) -> np.ndarray:
        return np.array([[self.fx, 0,        self.cx],
                         [0,       self.fy,  self.cy],
                         [0,       0,        1.0]], dtype=np.float64)


@dataclass
class StereoExtrinsic:
    """Rigid transform from RGB camera frame to thermal camera frame.

    R: 3x3 rotation, t: 3-vec translation in meters.
    For a typical co-mounted setup R ≈ I and t = (small_x, 0, 0).
    """
    R: np.ndarray = field(default_factory=lambda: np.eye(3))
    t: np.ndarray = field(default_factory=lambda: np.zeros(3))


def project_bbox_rgb_to_thermal(
        bbox_rgb: Tuple[int, int, int, int],
        depth_m: float,
        rgb: CameraCalib, thermal: CameraCalib,
        ext: StereoExtrinsic) -> Optional[Tuple[int, int, int, int]]:
    """Map an RGB bbox to thermal pixel coords assuming a single object depth.

    Args:
      bbox_rgb : (x1, y1, x2, y2) in RGB pixels
      depth_m  : approximate distance to the subject (e.g. from LRF / LiDAR)
      rgb, thermal : camera intrinsics
      ext      : RGB→thermal extrinsic

    Returns the projected bbox clipped to thermal image bounds, or None if
    the projection lies entirely outside the thermal image.
    """
    if depth_m <= 0.0:
        return None
    x1, y1, x2, y2 = bbox_rgb
    # Backproject the four corners to 3D in RGB camera frame at given depth
    corners_px = np.array([[x1, y1], [x2, y1], [x2, y2], [x1, y2]],
                          dtype=np.float64)
    K_inv = np.linalg.inv(rgb.K)
    pts_rgb_3d = []
    for u, v in corners_px:
        ray = K_inv @ np.array([u, v, 1.0])      # ray in RGB camera frame
        ray /= ray[2]                            # normalize so z = 1
        pts_rgb_3d.append(ray * depth_m)         # scale to depth
    pts_rgb_3d = np.asarray(pts_rgb_3d).T        # (3, 4)

    # Transform to thermal frame
    pts_thermal_3d = ext.R @ pts_rgb_3d + ext.t.reshape(3, 1)

    # Project to thermal pixels
    K_th = thermal.K
    proj = K_th @ pts_thermal_3d                  # (3, 4)
    proj[:2] /= proj[2:]                          # divide by depth
    xs = proj[0]
    ys = proj[1]

    tx1 = int(np.floor(xs.min()))
    ty1 = int(np.floor(ys.min()))
    tx2 = int(np.ceil(xs.max()))
    ty2 = int(np.ceil(ys.max()))

    # Clip to thermal image bounds
    tx1 = max(0, min(tx1, thermal.width - 1))
    tx2 = max(0, min(tx2, thermal.width - 1))
    ty1 = max(0, min(ty1, thermal.height - 1))
    ty2 = max(0, min(ty2, thermal.height - 1))

    if tx2 <= tx1 or ty2 <= ty1:
        return None     # bbox fell outside thermal FOV
    return tx1, ty1, tx2, ty2


# ────────── Thermal stats over a region ──────────
def thermal_stats_in_bbox(
        thermal_mono16: np.ndarray,
        bbox: Tuple[int, int, int, int],
        min_temp_c: float, max_temp_c: float,
) -> Optional[dict]:
    """Compute min/mean/max temperature inside the bbox.

    `thermal_mono16` is a (H, W) uint16 array. We linearly map raw [0, 65535]
    to [min_temp_c, max_temp_c] — most thermal cameras let you query this
    once at boot from their SDK.
    """
    if thermal_mono16.ndim != 2:
        return None
    x1, y1, x2, y2 = bbox
    if x2 <= x1 or y2 <= y1:
        return None
    region = thermal_mono16[y1:y2, x1:x2]
    if region.size == 0:
        return None
    raw_min = float(region.min())
    raw_max = float(region.max())
    raw_mean = float(region.mean())
    span = max_temp_c - min_temp_c
    scale = span / 65535.0
    return {
        "min_c":  min_temp_c + raw_min * scale,
        "mean_c": min_temp_c + raw_mean * scale,
        "max_c":  min_temp_c + raw_max * scale,
        "n_px": int(region.size),
    }


# ────────── High-level fuser used by PerceptionProcess ──────────
@dataclass
class FuserConfig:
    rgb_calib: CameraCalib
    thermal_calib: CameraCalib
    extrinsic: StereoExtrinsic = field(default_factory=StereoExtrinsic)
    sync_tolerance_s: float = 0.10
    default_depth_m: float   = 5.0


class ThermalRgbFuser:
    """Stateful syncer + projector. PerceptionProcess instantiates one."""

    def __init__(self, cfg: FuserConfig, thermal_buffer_size: int = 30):
        self.cfg = cfg
        self._thermal = TimeSyncBuffer(max_size=thermal_buffer_size)
        self.stats = {"queries": 0, "sync_miss": 0, "proj_miss": 0,
                      "ok": 0}

    def add_thermal_frame(self, stamp: float, mono16: np.ndarray) -> None:
        self._thermal.add(stamp, mono16)

    def enrich(self, rgb_stamp: float, bbox_rgb: Tuple[int, int, int, int],
               depth_m: Optional[float] = None,
               min_temp_c: float = -20.0, max_temp_c: float = 120.0
               ) -> Optional[dict]:
        """For an RGB-time anomaly bbox, return thermal stats or None.

        Returns dict with keys: min_c, mean_c, max_c, n_px, sync_dt_s, thermal_bbox.
        """
        self.stats["queries"] += 1
        match = self._thermal.nearest(rgb_stamp,
                                      max_dt_s=self.cfg.sync_tolerance_s)
        if match is None:
            self.stats["sync_miss"] += 1
            return None
        therm_stamp, therm_frame = match
        d = depth_m if depth_m and depth_m > 0 else self.cfg.default_depth_m
        proj = project_bbox_rgb_to_thermal(
            bbox_rgb, d,
            self.cfg.rgb_calib, self.cfg.thermal_calib, self.cfg.extrinsic,
        )
        if proj is None:
            self.stats["proj_miss"] += 1
            return None
        s = thermal_stats_in_bbox(therm_frame, proj,
                                  min_temp_c=min_temp_c, max_temp_c=max_temp_c)
        if s is None:
            self.stats["proj_miss"] += 1
            return None
        s["sync_dt_s"] = therm_stamp - rgb_stamp
        s["thermal_bbox"] = proj
        self.stats["ok"] += 1
        return s
