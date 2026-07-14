"""SAN v1.5 Phase 2-E Turn 11-12 — Sensor fusion math (pure logic).

Pinhole projection + RGB↔thermal bounding-box alignment.
No rclpy, no ROS — pytest-testable in isolation.

Ported from perception/thermal_rgb_fusion.py (legacy prototype),
trimmed to the essentials needed by PerceptionNode:
  * CameraCalib            — pinhole intrinsics
  * StereoExtrinsic        — RGB→thermal rigid transform
  * project_bbox_rgb_to_thermal()  — bbox transfer
  * thermal_stats_in_bbox()        — mean/max temp in a region
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional, Tuple

import numpy as np

# ─── Calibration ─────────────────────────────────────────────────────────

@dataclass
class CameraCalib:
    """Pinhole intrinsics. K = [[fx, 0, cx], [0, fy, cy], [0, 0, 1]]"""
    width: int
    height: int
    fx: float
    fy: float
    cx: float
    cy: float

    @property
    def K(self) -> np.ndarray:
        return np.array(
            [[self.fx, 0.0,     self.cx],
             [0.0,     self.fy, self.cy],
             [0.0,     0.0,     1.0]],
            dtype=np.float64,
        )


@dataclass
class StereoExtrinsic:
    """Rigid transform from RGB camera frame to thermal camera frame.

    R: 3x3 rotation, t: 3-vec translation in meters.
    For a co-mounted RGB/thermal pair typically R ≈ I and
    t = (small_x_baseline, 0, 0).
    """
    R: np.ndarray = field(
        default_factory=lambda: np.eye(3, dtype=np.float64))
    t: np.ndarray = field(
        default_factory=lambda: np.zeros(3, dtype=np.float64))


# ─── Bbox projection ─────────────────────────────────────────────────────

Bbox = Tuple[int, int, int, int]   # (x1, y1, x2, y2)


def project_bbox_rgb_to_thermal(
    bbox_rgb: Bbox,
    depth_m: float,
    rgb: CameraCalib,
    thermal: CameraCalib,
    ext: StereoExtrinsic,
) -> Optional[Bbox]:
    """Map an RGB bbox to thermal pixel coords at a given object depth.

    Returns the projected bbox clipped to thermal image bounds, or None
    if the projection falls entirely outside the thermal image.

    Algorithm:
      1. Backproject the 4 RGB-bbox corners to 3D rays.
      2. Multiply each ray by depth_m to obtain 3D points in RGB frame.
      3. Apply R, t to get 3D points in thermal frame.
      4. Project to thermal pixels via thermal.K.
      5. Take pixel-aligned bbox + clip.
    """
    if depth_m <= 0.0:
        return None

    x1, y1, x2, y2 = bbox_rgb
    if x2 <= x1 or y2 <= y1:
        return None

    corners = np.array(
        [[x1, y1], [x2, y1], [x2, y2], [x1, y2]],
        dtype=np.float64,
    )

    K_inv = np.linalg.inv(rgb.K)
    pts_rgb = []
    for u, v in corners:
        ray = K_inv @ np.array([u, v, 1.0])
        ray /= ray[2]                         # normalize z = 1
        pts_rgb.append(ray * depth_m)
    pts_rgb = np.asarray(pts_rgb).T            # (3, 4)

    pts_th = ext.R @ pts_rgb + ext.t.reshape(3, 1)
    proj   = thermal.K @ pts_th                # (3, 4)
    # Beware of points with non-positive Z (behind camera)
    if (proj[2] <= 0).any():
        return None
    proj[:2] /= proj[2:]

    tx1 = int(np.floor(proj[0].min()))
    ty1 = int(np.floor(proj[1].min()))
    tx2 = int(np.ceil(proj[0].max()))
    ty2 = int(np.ceil(proj[1].max()))

    # Clip
    tx1 = max(0, min(tx1, thermal.width - 1))
    tx2 = max(0, min(tx2, thermal.width - 1))
    ty1 = max(0, min(ty1, thermal.height - 1))
    ty2 = max(0, min(ty2, thermal.height - 1))

    if tx2 <= tx1 or ty2 <= ty1:
        return None
    return tx1, ty1, tx2, ty2


# ─── Thermal stats ───────────────────────────────────────────────────────

@dataclass
class ThermalStats:
    mean_c: float
    max_c: float
    valid: bool


def thermal_stats_in_bbox(
    thermal_image: np.ndarray,
    bbox: Bbox,
    raw_to_celsius_scale: float = 0.01,
    raw_to_celsius_offset: float = -273.15,
) -> ThermalStats:
    """Compute mean + max temperature (°C) inside a thermal bbox.

    `thermal_image` is uint16 (mono16) raw counts; conversion uses:
        celsius = raw * scale + offset
    Default scale/offset matches Centikelvin (0.01 K per count) which
    is common in mono16 FLIR outputs.

    Returns valid=False when the bbox is empty or out of bounds.
    """
    if thermal_image.ndim != 2:
        return ThermalStats(0.0, 0.0, False)
    h, w = thermal_image.shape
    x1, y1, x2, y2 = bbox
    # Entirely-outside check
    if x1 >= w or x2 <= 0 or y1 >= h or y2 <= 0:
        return ThermalStats(0.0, 0.0, False)
    x1 = max(0, min(x1, w - 1))
    x2 = max(0, min(x2, w))      # right-exclusive
    y1 = max(0, min(y1, h - 1))
    y2 = max(0, min(y2, h))
    if x2 <= x1 or y2 <= y1:
        return ThermalStats(0.0, 0.0, False)

    patch = thermal_image[y1:y2, x1:x2].astype(np.float64)
    if patch.size == 0:
        return ThermalStats(0.0, 0.0, False)
    mean_c = float(patch.mean() * raw_to_celsius_scale +
                   raw_to_celsius_offset)
    max_c  = float(patch.max()  * raw_to_celsius_scale +
                   raw_to_celsius_offset)
    return ThermalStats(mean_c, max_c, True)
