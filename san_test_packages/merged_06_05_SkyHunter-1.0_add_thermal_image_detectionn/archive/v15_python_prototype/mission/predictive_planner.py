"""PredictivePlanner — 1-second predictive broadcast (SDD Rev.A.6 §7.3).

10 Hz publish loop on the leader:
  매 100 ms:
    leader_path = AStar2D(P_L(t), goal, costmap)
    P_L(t+1.0)  = leader_path.sample_at(t+1.0, kinematic_speed)
    for each follower i:
      P_F_i(t+1.0) = P_L(t+1.0) + R(ψ) · offset_body_i  # V-shape offset
      publish FollowerTargetMessage(target_x, target_y, valid_until_ts)

KPP §2.1.1 「control latency ≤ 150 ms」 met by the followers consuming
their next target before they would otherwise drift onto the catch-up
ladder. The full Hybrid A* (with kinematic constraints + Reeds-Shepp
expansions) is deferred — Option α uses standard 8-connected grid A*
plus path-time sampling, sufficient for V-shape formation at 1.3 m/s
leader speed.
"""
from __future__ import annotations

import heapq
import math
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

import numpy as np


@dataclass
class FollowerTargetMessage:
    """Per-follower 1 s prediction message.

    Note: defined locally in this module rather than in core.messages so
    we can iterate on the schema without ripple-changing the rest of the
    codebase. The leader-side broadcast wraps these into the existing
    sw_follower_target topic when wiring lands.
    """
    follower_id: int
    target_x: float
    target_y: float
    target_heading_rad: float
    valid_until_ts: float           # monotonic seconds
    formation_id: int = 0
    leader_segment_id: Optional[int] = None


@dataclass
class GridCell:
    ix: int
    iy: int
    cost: float = 1.0
    h: float = 0.0
    parent: Optional["GridCell"] = None
    g: float = float("inf")

    def f(self) -> float:
        return self.g + self.h

    def __lt__(self, other: "GridCell") -> bool:
        return self.f() < other.f()


class AStar2D:
    """Standard 8-connected A* over a 2D occupancy grid (Option α)."""

    OBSTACLE_THRESHOLD = 0.95
    OBSTACLE_PENALTY = 5.0      # multiplier on cell cost (1 + 5·c)

    def __init__(self, costmap: np.ndarray, cell_size_m: float = 0.20):
        self.costmap = costmap
        self.cell_size_m = float(cell_size_m)
        self.h, self.w = costmap.shape

    def heuristic(self, ax: int, ay: int, bx: int, by: int) -> float:
        """Octile distance — admissible for 8-connected grids."""
        dx = abs(ax - bx)
        dy = abs(ay - by)
        return (dx + dy) + (math.sqrt(2.0) - 2.0) * min(dx, dy)

    def plan(self, start_xy: Tuple[float, float],
             goal_xy: Tuple[float, float],
             max_iters: int = 5000
             ) -> Optional[List[Tuple[float, float]]]:
        """Return list of (x, y) waypoints in world coords, or None."""
        sx, sy = self._world_to_grid(*start_xy)
        gx, gy = self._world_to_grid(*goal_xy)
        if not self._in_bounds(sx, sy) or not self._in_bounds(gx, gy):
            return None
        if (self.costmap[sy, sx] >= self.OBSTACLE_THRESHOLD or
                self.costmap[gy, gx] >= self.OBSTACLE_THRESHOLD):
            return None

        open_set: List[GridCell] = []
        cells: Dict[Tuple[int, int], GridCell] = {}
        start = GridCell(sx, sy, g=0.0,
                         h=self.heuristic(sx, sy, gx, gy))
        heapq.heappush(open_set, start)
        cells[(sx, sy)] = start

        iters = 0
        while open_set and iters < max_iters:
            iters += 1
            cur = heapq.heappop(open_set)
            if cur.ix == gx and cur.iy == gy:
                return self._reconstruct(cur)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    nx, ny = cur.ix + dx, cur.iy + dy
                    if not self._in_bounds(nx, ny):
                        continue
                    cost = float(self.costmap[ny, nx])
                    if cost >= self.OBSTACLE_THRESHOLD:
                        continue
                    step = math.hypot(dx, dy) * (1.0 + self.OBSTACLE_PENALTY * cost)
                    g_new = cur.g + step
                    nb = cells.get((nx, ny))
                    if nb is None:
                        nb = GridCell(nx, ny, cost=cost,
                                      h=self.heuristic(nx, ny, gx, gy))
                        cells[(nx, ny)] = nb
                    if g_new < nb.g:
                        nb.g = g_new
                        nb.parent = cur
                        heapq.heappush(open_set, nb)
        return None

    def _in_bounds(self, ix: int, iy: int) -> bool:
        return 0 <= ix < self.w and 0 <= iy < self.h

    def _world_to_grid(self, x: float, y: float) -> Tuple[int, int]:
        return int(x / self.cell_size_m), int(y / self.cell_size_m)

    def _grid_to_world(self, ix: int, iy: int) -> Tuple[float, float]:
        half = self.cell_size_m / 2.0
        return (ix * self.cell_size_m + half,
                iy * self.cell_size_m + half)

    def _reconstruct(self,
                     end: GridCell) -> List[Tuple[float, float]]:
        path: List[Tuple[float, float]] = []
        cur: Optional[GridCell] = end
        while cur is not None:
            path.append(self._grid_to_world(cur.ix, cur.iy))
            cur = cur.parent
        return list(reversed(path))


class PredictivePlanner:
    """Compute leader prediction at t+1 s and per-follower targets."""

    LOOKAHEAD_S = 1.0
    DEFAULT_LEADER_SPEED = 1.3   # m/s

    def __init__(self,
                 formation_offsets: Dict[int, Tuple[float, float]]):
        """formation_offsets: { follower_id: (offset_x_body_m, offset_y_body_m) }

        Body frame: x = forward (along leader heading), y = left.
        V-shape example for d_nominal = 5 m, theta = 90°:
          { 1: (-3.5, +3.5),   # rear-left
            2: (-3.5, -3.5),   # rear-right
            3: (-7.0, +7.0),   # 2nd row left
            4: (-7.0, -7.0) }
        """
        self.formation_offsets = dict(formation_offsets)

    def predict_leader(self,
                       current_pose: Tuple[float, float, float],
                       path: List[Tuple[float, float]],
                       speed_mps: float = DEFAULT_LEADER_SPEED
                       ) -> Tuple[Tuple[float, float], float]:
        """Sample path at t + LOOKAHEAD_S along the planned path.

        Returns ((x, y), heading_rad) at t+1.
        """
        if not path or len(path) < 2:
            x, y, h = current_pose
            return ((x, y), h)
        target_dist = speed_mps * self.LOOKAHEAD_S
        cumulative = 0.0
        for i in range(1, len(path)):
            seg = math.hypot(path[i][0] - path[i - 1][0],
                             path[i][1] - path[i - 1][1])
            if cumulative + seg >= target_dist:
                t = (target_dist - cumulative) / seg if seg > 0 else 0.0
                x = path[i - 1][0] + t * (path[i][0] - path[i - 1][0])
                y = path[i - 1][1] + t * (path[i][1] - path[i - 1][1])
                heading = math.atan2(path[i][1] - path[i - 1][1],
                                     path[i][0] - path[i - 1][0])
                return ((x, y), heading)
            cumulative += seg
        # Path ended before lookahead — hold the last point, last heading.
        last = len(path) - 1
        prev = max(0, last - 1)
        heading = math.atan2(path[last][1] - path[prev][1],
                             path[last][0] - path[prev][0])
        return (path[last], heading)

    def compute_follower_targets(self,
                                 leader_pred: Tuple[float, float],
                                 leader_heading_rad: float,
                                 valid_until_ts: float,
                                 formation_id: int = 0
                                 ) -> List[FollowerTargetMessage]:
        """For each follower, target = P_L + R(ψ) · offset_body."""
        cos_h = math.cos(leader_heading_rad)
        sin_h = math.sin(leader_heading_rad)
        out: List[FollowerTargetMessage] = []
        for follower_id, (ox, oy) in self.formation_offsets.items():
            wx = ox * cos_h - oy * sin_h
            wy = ox * sin_h + oy * cos_h
            out.append(FollowerTargetMessage(
                follower_id=follower_id,
                target_x=leader_pred[0] + wx,
                target_y=leader_pred[1] + wy,
                target_heading_rad=leader_heading_rad,
                valid_until_ts=valid_until_ts,
                formation_id=formation_id,
            ))
        return out
