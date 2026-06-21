"""Tests for SlamPersistentLayer (P1-7, SDD Rev.A.6 §4.7.5)."""
import numpy as np
import pytest

from mapping.slam_persistent_layer import SlamPersistentLayer


def test_init_starts_at_uncertain():
    layer = SlamPersistentLayer(grid_shape=(10, 10))
    assert (layer.grid == 0.5).all()


def test_bayesian_converges_to_obstacle():
    """20 observations of P=1.0 at α=0.95 should pull the mean above 0.6.

    Math: starting at 0.5, after k iterations with P_obs=1.0 the cell
    value is 1 - 0.5·α^k. After 20 iters: 1 - 0.5·0.95^20 ≈ 0.821."""
    layer = SlamPersistentLayer(grid_shape=(5, 5))
    obs = np.ones((5, 5), dtype=np.float32)
    for _ in range(20):
        layer.bayesian_update(obs)
    assert layer.grid.mean() > 0.6


def test_bayesian_50_iters_converges_strongly():
    layer = SlamPersistentLayer(grid_shape=(5, 5))
    obs = np.ones((5, 5), dtype=np.float32)
    for _ in range(50):
        layer.bayesian_update(obs)
    assert layer.grid.mean() > 0.9


def test_shape_mismatch_raises():
    layer = SlamPersistentLayer(grid_shape=(5, 5))
    with pytest.raises(ValueError):
        layer.bayesian_update(np.ones((3, 3), dtype=np.float32))


def test_clip_to_unit_range():
    layer = SlamPersistentLayer(grid_shape=(3, 3))
    layer.grid = np.full((3, 3), 0.95, dtype=np.float32)
    layer.bayesian_update(np.full((3, 3), 1.5, dtype=np.float32))  # invalid
    assert layer.grid.max() <= 1.0


def test_reset_to_uncertain():
    layer = SlamPersistentLayer(grid_shape=(5, 5))
    layer.grid.fill(0.9)
    layer.reset()
    assert (layer.grid == 0.5).all()
