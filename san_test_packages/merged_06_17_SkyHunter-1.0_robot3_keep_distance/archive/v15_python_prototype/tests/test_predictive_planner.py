"""Tests for AStar2D + PredictivePlanner (P1-3, SDD Rev.A.6 §7.3)."""
from __future__ import annotations

import math
import time

import numpy as np

from mission.predictive_planner import AStar2D, PredictivePlanner


# ───── A* tests ─────
def test_astar_straight_line_in_free_space():
    cm = np.zeros((20, 20), dtype=np.float32)
    a = AStar2D(cm, cell_size_m=0.20)
    path = a.plan((0.5, 0.5), (3.5, 0.5))
    assert path is not None
    assert len(path) > 0


def test_astar_no_path_through_full_wall():
    cm = np.zeros((20, 20), dtype=np.float32)
    cm[:, 10] = 1.0   # full vertical wall
    a = AStar2D(cm, cell_size_m=0.20)
    path = a.plan((0.5, 1.0), (3.5, 1.0))
    assert path is None


def test_astar_finds_path_around_obstacle():
    cm = np.zeros((20, 20), dtype=np.float32)
    cm[:15, 10] = 1.0   # wall with gap at top (rows 15..19 free)
    a = AStar2D(cm, cell_size_m=0.20)
    path = a.plan((0.5, 0.5), (3.5, 0.5))
    assert path is not None


def test_astar_start_in_obstacle_returns_none():
    cm = np.zeros((10, 10), dtype=np.float32)
    cm[5, 5] = 1.0
    a = AStar2D(cm, cell_size_m=0.20)
    # World (1.1, 1.1) → cell (5, 5) which is the obstacle
    path = a.plan((1.1, 1.1), (0.1, 0.1))
    assert path is None


# ───── Predictive lookahead tests ─────
def test_lookahead_at_t1_with_constant_speed():
    pp = PredictivePlanner(formation_offsets={1: (-3.0, 0.0)})
    # 1.3 m/s × 1.0 s = 1.3 m forward along straight east path.
    path = [(0.0, 0.0), (5.0, 0.0)]
    (px, py), h = pp.predict_leader((0.0, 0.0, 0.0), path, speed_mps=1.3)
    assert abs(px - 1.3) < 0.01
    assert abs(py - 0.0) < 0.01
    assert abs(h - 0.0) < 0.01


def test_lookahead_path_too_short():
    pp = PredictivePlanner(formation_offsets={1: (-3.0, 0.0)})
    path = [(0.0, 0.0), (0.5, 0.0)]   # only 0.5 m
    (px, py), _h = pp.predict_leader((0.0, 0.0, 0.0), path, speed_mps=1.3)
    # Should clamp to last point
    assert px == 0.5


def test_lookahead_curved_path():
    pp = PredictivePlanner(formation_offsets={1: (-3.0, 0.0)})
    # 90° turn after 0.5 m forward
    path = [(0.0, 0.0), (0.5, 0.0), (0.5, 1.0)]
    (px, py), h = pp.predict_leader((0.0, 0.0, 0.0), path, speed_mps=1.3)
    # Expected: 0.5 m forward + 0.8 m north
    assert abs(px - 0.5) < 0.05
    assert abs(py - 0.8) < 0.05
    # Heading at t+1 is 90° (north)
    assert abs(h - math.pi / 2) < 0.1


# ───── Follower target tests ─────
def test_follower_target_no_rotation():
    """Leader heading = 0 (east) → body offset == world offset."""
    offsets = {1: (-3.0, 1.5), 2: (-3.0, -1.5)}
    pp = PredictivePlanner(formation_offsets=offsets)
    deadline = time.monotonic() + 1.0
    targets = pp.compute_follower_targets(
        leader_pred=(10.0, 5.0),
        leader_heading_rad=0.0,
        valid_until_ts=deadline,
    )
    assert len(targets) == 2
    f1 = next(t for t in targets if t.follower_id == 1)
    assert abs(f1.target_x - 7.0) < 0.01     # 10 + (-3)
    assert abs(f1.target_y - 6.5) < 0.01     # 5 + 1.5


def test_follower_target_rotation_90deg():
    """Leader heading = 90° (north) → body x → world y, body y → -world x."""
    offsets = {1: (-3.0, 0.0)}
    pp = PredictivePlanner(formation_offsets=offsets)
    targets = pp.compute_follower_targets(
        leader_pred=(10.0, 5.0),
        leader_heading_rad=math.pi / 2,
        valid_until_ts=time.monotonic() + 1.0,
    )
    f1 = targets[0]
    # body (-3, 0) rotated 90° CCW = world (0, -3) → target (10, 2)
    assert abs(f1.target_x - 10.0) < 0.01
    assert abs(f1.target_y - 2.0) < 0.01


def test_follower_target_includes_valid_until():
    pp = PredictivePlanner(formation_offsets={1: (-3.0, 0.0)})
    deadline = time.monotonic() + 1.0
    targets = pp.compute_follower_targets(
        leader_pred=(0.0, 0.0),
        leader_heading_rad=0.0,
        valid_until_ts=deadline,
    )
    assert targets[0].valid_until_ts == deadline


def test_v_shape_4_followers():
    """V-shape formation: 4 followers behind leader."""
    offsets = {
        1: (-3.5, 3.5),    # rear-left (closer)
        2: (-3.5, -3.5),   # rear-right
        3: (-7.0, 7.0),    # 2nd row left
        4: (-7.0, -7.0),   # 2nd row right
    }
    pp = PredictivePlanner(formation_offsets=offsets)
    targets = pp.compute_follower_targets(
        leader_pred=(0.0, 0.0),
        leader_heading_rad=0.0,
        valid_until_ts=time.monotonic() + 1.0,
    )
    assert len(targets) == 4
    # All should be behind leader (negative x in world frame at heading 0).
    for t in targets:
        assert t.target_x < 0.0
