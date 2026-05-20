"""Hungarian slot assignment for formation transitions (SDD §15.4, P2-2).

When formation changes from A to B, optimally pair each follower's current
position with target slot to minimize total movement (Hungarian algorithm).

Without this, simple index pairing causes path crossings and wasted motion.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Optional, Tuple

import numpy as np


@dataclass
class SlotAssignment:
    follower_id: int
    target_slot_idx: int
    cost: float


class HungarianAssigner:
    """Optimal pairing of follower positions to formation slots."""

    @staticmethod
    def build_cost_matrix(current_positions: List[Tuple[float, float]],
                          target_slots: List[Tuple[float, float]]
                          ) -> np.ndarray:
        n = len(current_positions)
        m = len(target_slots)
        cost = np.zeros((n, m), dtype=np.float64)
        for i, p in enumerate(current_positions):
            for j, s in enumerate(target_slots):
                cost[i, j] = math.hypot(p[0] - s[0], p[1] - s[1])
        return cost

    @classmethod
    def assign(cls,
               current_positions: List[Tuple[float, float]],
               target_slots: List[Tuple[float, float]],
               follower_ids: Optional[List[int]] = None
               ) -> List[SlotAssignment]:
        """Optimal pairing minimizing total cost.

        Returns list of (follower_id, target_slot_idx, cost). Caller iterates
        and assigns each follower to slot at index target_slot_idx.
        """
        if not current_positions or not target_slots:
            return []
        if len(current_positions) != len(target_slots):
            raise ValueError(
                f"size mismatch: "
                f"{len(current_positions)} vs {len(target_slots)}"
            )

        try:
            from scipy.optimize import linear_sum_assignment
        except ImportError:
            return cls._greedy_assign(current_positions, target_slots,
                                      follower_ids)

        cost = cls.build_cost_matrix(current_positions, target_slots)
        row_ind, col_ind = linear_sum_assignment(cost)
        ids = follower_ids or list(range(len(current_positions)))
        result = []
        for r, c in zip(row_ind, col_ind, strict=False):
            result.append(SlotAssignment(
                follower_id=ids[int(r)],
                target_slot_idx=int(c),
                cost=float(cost[r, c]),
            ))
        return result

    @classmethod
    def _greedy_assign(cls,
                       current_positions: List[Tuple[float, float]],
                       target_slots: List[Tuple[float, float]],
                       follower_ids: Optional[List[int]]
                       ) -> List[SlotAssignment]:
        """Fallback when scipy unavailable. Suboptimal but works."""
        ids = follower_ids or list(range(len(current_positions)))
        cost = cls.build_cost_matrix(current_positions, target_slots)
        n = len(current_positions)
        used_slots: set = set()
        result = []
        order = sorted(range(n), key=lambda i: cost[i].min())
        for i in order:
            available = [j for j in range(n) if j not in used_slots]
            if not available:
                break
            best_j = min(available, key=lambda j: cost[i, j])
            used_slots.add(best_j)
            result.append(SlotAssignment(
                follower_id=ids[i],
                target_slot_idx=best_j,
                cost=float(cost[i, best_j]),
            ))
        return result

    @classmethod
    def total_cost(cls, assignments: List[SlotAssignment]) -> float:
        return sum(a.cost for a in assignments)
