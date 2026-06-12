"""Tests for Hungarian slot assignment (P2-2)."""
from __future__ import annotations

import time

import pytest

from swarm.slot_assignment import HungarianAssigner


def test_optimal_pairing_simple():
    """3 followers, target slots aligned → identity assignment is optimal."""
    current = [(0, 0), (1, 0), (2, 0)]
    target = [(0, 5), (1, 5), (2, 5)]
    result = HungarianAssigner.assign(current, target)
    assert len(result) == 3
    pairs = {a.follower_id: a.target_slot_idx for a in result}
    assert pairs == {0: 0, 1: 1, 2: 2}


def test_avoids_crossing():
    """Two followers positioned identically → identity, not swap."""
    current = [(0, 0), (10, 0)]
    target = [(0, 5), (10, 5)]
    result = HungarianAssigner.assign(current, target)
    pairs = {a.follower_id: a.target_slot_idx for a in result}
    assert pairs[0] == 0
    assert pairs[1] == 1


def test_distance_minimized():
    current = [(0, 0), (10, 0), (5, 5)]
    target = [(0, 1), (10, 1), (5, 6)]
    result = HungarianAssigner.assign(current, target)
    total = HungarianAssigner.total_cost(result)
    assert total < 5.0  # near optimal (each ~1m vertical)


def test_with_follower_ids():
    current = [(0, 0), (5, 0)]
    target = [(0, 5), (5, 5)]
    result = HungarianAssigner.assign(current, target, follower_ids=[101, 202])
    ids = {a.follower_id for a in result}
    assert ids == {101, 202}


def test_empty_input():
    result = HungarianAssigner.assign([], [])
    assert result == []


def test_size_mismatch_raises():
    with pytest.raises(ValueError):
        HungarianAssigner.assign([(0, 0), (1, 0)], [(0, 5)])


def test_cost_matrix_shape():
    cost = HungarianAssigner.build_cost_matrix(
        [(0, 0), (1, 0), (2, 0)],
        [(0, 1), (1, 1), (2, 1)])
    assert cost.shape == (3, 3)
    assert abs(cost[0, 0] - 1.0) < 1e-9


def test_n9_completes_quickly():
    """9 followers (max swarm size) — should complete < 50 ms."""
    current = [(i, 0) for i in range(9)]
    target = [(i, 10) for i in range(9)]
    start = time.monotonic()
    result = HungarianAssigner.assign(current, target)
    elapsed = time.monotonic() - start
    assert len(result) == 9
    assert elapsed < 0.05, f"Hungarian N=9 took {elapsed*1000:.1f} ms"


def test_greedy_fallback_works():
    current = [(0, 0), (5, 0), (10, 0)]
    target = [(0, 5), (5, 5), (10, 5)]
    result = HungarianAssigner._greedy_assign(current, target, [0, 1, 2])
    assert len(result) == 3
    used = {a.target_slot_idx for a in result}
    assert used == {0, 1, 2}


def test_total_cost_calculation():
    current = [(0, 0)]
    target = [(3, 4)]  # distance 5
    result = HungarianAssigner.assign(current, target)
    assert abs(HungarianAssigner.total_cost(result) - 5.0) < 1e-6
