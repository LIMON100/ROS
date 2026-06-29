"""Hybrid static layer update — SLAM → OSM (SDD §4.7.6, P2-5).

When the SLAM persistent layer has high confidence (>= 0.9 obstacle or
<= 0.1 free) over many observations (>= 100), promote that finding into
the OSM static layer. Addresses OSM stale data without waiting for an
upstream OSM contributor.

Workflow:
1. SLAM persistent cell observation_count >= 100
2. SLAM value >= 0.9 (confirmed obstacle) or <= 0.1 (confirmed free)
3. abs(SLAM − OSM) >= 0.3 (only promote when there's a real disagreement)
4. Update OsmStaticLayer cell value in-place
5. Mission end: upload updated OSM tiles to patrol_server (Phase F+)
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

import numpy as np

from mapping.osm_static_layer import OsmStaticLayer
from mapping.slam_persistent_layer import SlamPersistentLayer


@dataclass
class HybridUpdate:
    cell_x_idx: int
    cell_y_idx: int
    old_static: float
    new_static: float
    slam_value: float
    obs_count: int


class HybridLayerUpdater:
    """Promote SLAM-confirmed cells into the OSM static layer."""

    DEFAULT_MIN_OBSERVATIONS = 100
    DEFAULT_MIN_OBSTACLE_CONF = 0.9
    DEFAULT_MAX_FREE_CONF = 0.1
    DEFAULT_OSM_DIFF_THRESHOLD = 0.3

    def __init__(self,
                 min_observations: int = DEFAULT_MIN_OBSERVATIONS,
                 min_obstacle_conf: float = DEFAULT_MIN_OBSTACLE_CONF,
                 max_free_conf: float = DEFAULT_MAX_FREE_CONF,
                 osm_diff_threshold: float = DEFAULT_OSM_DIFF_THRESHOLD):
        self.min_observations = min_observations
        self.min_obstacle_conf = min_obstacle_conf
        self.max_free_conf = max_free_conf
        self.osm_diff_threshold = osm_diff_threshold
        self._updated_cells: List[HybridUpdate] = []
        self._last_update_mask: Optional[np.ndarray] = None

    def evaluate_and_update(self,
                            osm_layer: OsmStaticLayer,
                            slam_layer: SlamPersistentLayer
                            ) -> List[HybridUpdate]:
        """Run periodically (e.g. every 60 s). Returns updates emitted on
        this call. Modifies osm_layer.grid in-place."""
        if osm_layer.grid is None or slam_layer.grid is None:
            return []
        if osm_layer.grid.shape != slam_layer.grid.shape:
            return []

        s = osm_layer.grid
        p = slam_layer.grid
        c = slam_layer.observation_count

        confident_mask = (c >= self.min_observations) & (
            (p >= self.min_obstacle_conf) | (p <= self.max_free_conf)
        )
        diff_mask = np.abs(p - s) >= self.osm_diff_threshold
        update_mask = confident_mask & diff_mask
        self._last_update_mask = update_mask

        updates: List[HybridUpdate] = []
        rows, cols = np.where(update_mask)
        for iy, ix in zip(rows.tolist(), cols.tolist(), strict=True):
            old_val = float(s[iy, ix])
            new_val = float(p[iy, ix])
            updates.append(HybridUpdate(
                cell_x_idx=int(ix), cell_y_idx=int(iy),
                old_static=old_val, new_static=new_val,
                slam_value=new_val, obs_count=int(c[iy, ix]),
            ))
            s[iy, ix] = new_val

        self._updated_cells.extend(updates)
        return updates

    def get_updates_summary(self) -> dict:
        if not self._updated_cells:
            return {"total": 0, "obstacles_added": 0, "free_added": 0}
        n = len(self._updated_cells)
        obs = sum(1 for u in self._updated_cells if u.new_static >= 0.9)
        free = sum(1 for u in self._updated_cells if u.new_static <= 0.1)
        return {
            "total": n,
            "obstacles_added": obs,
            "free_added": free,
            "avg_obs_count": float(
                np.mean([u.obs_count for u in self._updated_cells])),
        }

    def get_audit_entries(self) -> List[dict]:
        """Format for audit log (P1-16)."""
        return [{
            "cell": (u.cell_x_idx, u.cell_y_idx),
            "old": round(u.old_static, 3),
            "new": round(u.new_static, 3),
            "obs_count": u.obs_count,
        } for u in self._updated_cells]
