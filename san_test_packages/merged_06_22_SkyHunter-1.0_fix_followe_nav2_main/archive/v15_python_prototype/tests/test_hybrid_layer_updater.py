"""Tests for hybrid OSM-SLAM update (P2-5)."""
from __future__ import annotations

import numpy as np

from mapping.hybrid_layer_updater import HybridLayerUpdater
from mapping.osm_static_layer import OsmStaticLayer
from mapping.slam_persistent_layer import SlamPersistentLayer


def _make_layers(shape=(10, 10)):
    osm = OsmStaticLayer()
    osm.grid = np.full(shape, 0.5, dtype=np.float32)
    slam = SlamPersistentLayer(grid_shape=shape)
    return osm, slam


def test_no_update_when_obs_count_low():
    osm, slam = _make_layers()
    osm.grid[5, 5] = 0.5
    slam.grid[5, 5] = 0.95
    slam.observation_count[5, 5] = 50  # below threshold
    updater = HybridLayerUpdater(min_observations=100)
    updates = updater.evaluate_and_update(osm, slam)
    assert updates == []
    assert osm.grid[5, 5] == 0.5


def test_update_when_thresholds_met():
    osm, slam = _make_layers()
    osm.grid[5, 5] = 0.3
    slam.grid[5, 5] = 0.95
    slam.observation_count[5, 5] = 100
    updater = HybridLayerUpdater(min_observations=100,
                                 min_obstacle_conf=0.9,
                                 osm_diff_threshold=0.3)
    updates = updater.evaluate_and_update(osm, slam)
    assert len(updates) == 1
    assert abs(updates[0].old_static - 0.3) < 1e-5
    assert abs(updates[0].new_static - 0.95) < 1e-5
    assert abs(osm.grid[5, 5] - 0.95) < 1e-5


def test_no_update_when_osm_already_agrees():
    osm, slam = _make_layers()
    osm.grid[5, 5] = 0.93
    slam.grid[5, 5] = 0.95
    slam.observation_count[5, 5] = 100
    updater = HybridLayerUpdater(osm_diff_threshold=0.3)
    updates = updater.evaluate_and_update(osm, slam)
    assert updates == []


def test_free_promotion():
    """OSM building → SLAM confirms free → promote."""
    osm, slam = _make_layers()
    osm.grid[3, 3] = 0.95
    slam.grid[3, 3] = 0.05
    slam.observation_count[3, 3] = 150
    updater = HybridLayerUpdater()
    updates = updater.evaluate_and_update(osm, slam)
    assert len(updates) == 1
    assert osm.grid[3, 3] < 0.1


def test_uncertain_slam_not_promoted():
    osm, slam = _make_layers()
    osm.grid[5, 5] = 0.1
    slam.grid[5, 5] = 0.6  # uncertain
    slam.observation_count[5, 5] = 100
    updater = HybridLayerUpdater()
    updates = updater.evaluate_and_update(osm, slam)
    assert updates == []


def test_summary_format():
    osm, slam = _make_layers()
    osm.grid[1, 1] = 0.1
    slam.grid[1, 1] = 0.95
    slam.observation_count[1, 1] = 100
    osm.grid[2, 2] = 0.95
    slam.grid[2, 2] = 0.05
    slam.observation_count[2, 2] = 100
    updater = HybridLayerUpdater()
    updater.evaluate_and_update(osm, slam)
    summary = updater.get_updates_summary()
    assert summary["total"] == 2
    assert summary["obstacles_added"] == 1
    assert summary["free_added"] == 1


def test_audit_entries_format():
    osm, slam = _make_layers()
    osm.grid[1, 1] = 0.1
    slam.grid[1, 1] = 0.95
    slam.observation_count[1, 1] = 120
    updater = HybridLayerUpdater()
    updater.evaluate_and_update(osm, slam)
    entries = updater.get_audit_entries()
    assert len(entries) == 1
    e = entries[0]
    assert e["cell"] == (1, 1)
    assert e["obs_count"] == 120
    assert "old" in e and "new" in e


def test_shape_mismatch_returns_empty():
    osm = OsmStaticLayer()
    osm.grid = np.zeros((5, 5), dtype=np.float32)
    slam = SlamPersistentLayer(grid_shape=(10, 10))
    updater = HybridLayerUpdater()
    assert updater.evaluate_and_update(osm, slam) == []


def test_observation_count_increments_only_on_signal():
    """Confident readings (|p − 0.5| >= OBS_SIGNAL_MARGIN) advance the
    counter; mid-range readings do not."""
    slam = SlamPersistentLayer(grid_shape=(5, 5))
    # Mid-range reading — should NOT advance the counter (|0.8 − 0.5| = 0.3 <
    # default OBS_SIGNAL_MARGIN=0.4).
    slam.bayesian_update(np.full((5, 5), 0.8, dtype=np.float32))
    assert slam.observation_count[2, 2] == 0
    # Strong reading.
    slam.bayesian_update(np.full((5, 5), 0.95, dtype=np.float32))
    assert slam.observation_count[2, 2] == 1
    # Reset clears the counter.
    slam.reset()
    assert slam.observation_count[2, 2] == 0
