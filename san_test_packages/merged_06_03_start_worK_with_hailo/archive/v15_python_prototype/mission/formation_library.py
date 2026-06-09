"""9 formation types library (SDD §7.2, P2-1).

Each function returns body-frame offsets [(x_back, y_lateral), ...] for N
followers given d (spacing) and theta (angle, where applicable).

Body frame convention:
  +x = forward (leader's heading direction)
  +y = left of leader
  Origin = leader position

PredictivePlanner consumes these via formation_offsets dict.
"""
from __future__ import annotations

import math
import random
from enum import Enum
from typing import Dict, List, Tuple


class FormationType(str, Enum):
    COLUMN = "column"               # 1열 종대
    LINE = "line"                   # 1열 횡대
    V_SHAPE = "v_shape"             # V형
    DIAMOND = "diamond"             # 다이아몬드
    ECHELON_LEFT = "echelon_left"   # 에셸론 좌
    ECHELON_RIGHT = "echelon_right" # 에셸론 우
    BOX = "box"                     # 박스
    VEE_INVERTED = "vee_inverted"   # 역 V형
    FREE_SPREAD = "free_spread"     # 자유 산개


class FormationLibrary:
    """Static methods to compute body-frame offsets for each formation."""

    @staticmethod
    def column(n: int, d: float, **kwargs) -> List[Tuple[float, float]]:
        """1열 종대 — single file behind leader on x-axis."""
        return [(-i * d, 0.0) for i in range(1, n + 1)]

    @staticmethod
    def line(n: int, d: float, **kwargs) -> List[Tuple[float, float]]:
        """1열 횡대 — spread laterally beside leader (slightly behind)."""
        offsets = []
        for i in range(n):
            side = 1 if i % 2 == 0 else -1
            row_idx = (i // 2) + 1
            offsets.append((-d * 0.5, side * row_idx * d))
        return offsets

    @staticmethod
    def v_shape(n: int, d: float, theta_deg: float = 90.0,
                **kwargs) -> List[Tuple[float, float]]:
        """V형 — opening backward, V apex at leader.

        theta_deg: total V angle (e.g. 90°). Half-angle from x-axis = theta/2.
        """
        offsets = []
        half = math.radians(theta_deg / 2.0)
        for i in range(n):
            side = 1 if i % 2 == 0 else -1
            row = (i // 2) + 1
            x = -row * d * math.cos(half)
            y = side * row * d * math.sin(half)
            offsets.append((x, y))
        return offsets

    @staticmethod
    def diamond(n: int, d: float, **kwargs) -> List[Tuple[float, float]]:
        """다이아몬드 — 4 around leader (front, left, back, right).

        For N=4 ideal. N>4 wraps around in 2nd ring.
        """
        offsets = []
        cardinals = [(d, 0.0), (0.0, d), (-d, 0.0), (0.0, -d)]
        for i in range(min(n, 4)):
            offsets.append(cardinals[i])
        if n > 4:
            outer_count = n - 4
            for j in range(outer_count):
                angle = math.radians(45 + j * (360.0 / outer_count))
                offsets.append((2 * d * math.cos(angle),
                                2 * d * math.sin(angle)))
        return offsets

    @staticmethod
    def echelon_left(n: int, d: float, theta_deg: float = 45.0,
                     **kwargs) -> List[Tuple[float, float]]:
        """에셸론 좌 — diagonal back-left."""
        offsets = []
        theta = math.radians(theta_deg)
        for i in range(1, n + 1):
            offsets.append((-i * d * math.cos(theta),
                            i * d * math.sin(theta)))
        return offsets

    @staticmethod
    def echelon_right(n: int, d: float, theta_deg: float = 45.0,
                      **kwargs) -> List[Tuple[float, float]]:
        """에셸론 우 — diagonal back-right (mirror of echelon_left)."""
        offsets = []
        theta = math.radians(theta_deg)
        for i in range(1, n + 1):
            offsets.append((-i * d * math.cos(theta),
                            -i * d * math.sin(theta)))
        return offsets

    @staticmethod
    def box(n: int, d: float, **kwargs) -> List[Tuple[float, float]]:
        """박스 — square corners (best for N=4)."""
        offsets = []
        corners = [
            (d / 2, d / 2),
            (-d / 2, d / 2),
            (-d / 2, -d / 2),
            (d / 2, -d / 2),
        ]
        for i in range(min(n, 4)):
            offsets.append(corners[i])
        if n > 4:
            edge_mids = [
                (d / 2, 0.0),
                (0.0, d / 2),
                (-d / 2, 0.0),
                (0.0, -d / 2),
            ]
            for j in range(min(n - 4, 4)):
                offsets.append(edge_mids[j])
            for j in range(n - 8):
                a = j % 4
                offsets.append((corners[a][0] * 2, corners[a][1] * 2))
        return offsets

    @staticmethod
    def vee_inverted(n: int, d: float, theta_deg: float = 90.0,
                     **kwargs) -> List[Tuple[float, float]]:
        """역 V형 — opening forward (used for ambush/assault)."""
        offsets = []
        half = math.radians(theta_deg / 2.0)
        for i in range(n):
            side = 1 if i % 2 == 0 else -1
            row = (i // 2) + 1
            x = +row * d * math.cos(half)
            y = side * row * d * math.sin(half)
            offsets.append((x, y))
        return offsets

    @staticmethod
    def free_spread(n: int, d: float, area_radius: float = 10.0,
                    seed: int = 42,
                    **kwargs) -> List[Tuple[float, float]]:
        """자유 산개 — pseudo-random within annulus (deterministic via seed)."""
        rnd = random.Random(seed)
        offsets = []
        min_r = max(d, 1.0)
        for _ in range(n):
            r = rnd.uniform(min_r, area_radius)
            theta = rnd.uniform(0, 2 * math.pi)
            offsets.append((r * math.cos(theta), r * math.sin(theta)))
        return offsets

    @classmethod
    def compute(cls,
                formation_type: FormationType,
                n_followers: int,
                d_m: float = 5.0,
                theta_deg: float = 90.0,
                **kwargs) -> List[Tuple[float, float]]:
        """Dispatch to specific formation function."""
        if n_followers <= 0:
            return []
        method_map = {
            FormationType.COLUMN:        cls.column,
            FormationType.LINE:          cls.line,
            FormationType.V_SHAPE:       cls.v_shape,
            FormationType.DIAMOND:       cls.diamond,
            FormationType.ECHELON_LEFT:  cls.echelon_left,
            FormationType.ECHELON_RIGHT: cls.echelon_right,
            FormationType.BOX:           cls.box,
            FormationType.VEE_INVERTED:  cls.vee_inverted,
            FormationType.FREE_SPREAD:   cls.free_spread,
        }
        fn = method_map.get(formation_type)
        if fn is None:
            raise ValueError(f"Unknown formation type: {formation_type}")
        return fn(n_followers, d_m, theta_deg=theta_deg, **kwargs)

    @staticmethod
    def to_planner_dict(offsets: List[Tuple[float, float]],
                        start_id: int = 1
                        ) -> Dict[int, Tuple[float, float]]:
        """Convert list to PredictivePlanner.formation_offsets format."""
        return {start_id + i: off for i, off in enumerate(offsets)}
