"""Tests for anomaly detection (P1-8, SDD Rev.A.6 §4.7.5)."""
import numpy as np

from mapping.osm_static_layer import OsmStaticLayer
from mapping.processes import MapFusionProcess
from mapping.slam_persistent_layer import SlamPersistentLayer


def make_layers(shape=(5, 5)):
    osm = OsmStaticLayer()
    osm.grid = np.full(shape, 0.5, dtype=np.float32)
    slam = SlamPersistentLayer(grid_shape=shape)
    return osm, slam


def test_no_anomaly_when_both_uncertain():
    osm, slam = make_layers()
    anomalies = MapFusionProcess.detect_anomaly(osm, slam, threshold=0.7)
    assert anomalies == []


def test_unmapped_obstacle_detection():
    osm, slam = make_layers()
    osm.grid[2, 2] = 0.1   # OSM says free
    slam.grid[2, 2] = 0.9  # SLAM says obstacle
    anomalies = MapFusionProcess.detect_anomaly(osm, slam, threshold=0.7)
    assert len(anomalies) == 1
    assert anomalies[0]["type"] == "unmapped_obstacle"
    assert anomalies[0]["confidence"] > 0.7


def test_structure_changed_detection():
    osm, slam = make_layers()
    osm.grid[1, 1] = 0.95  # OSM says building
    slam.grid[1, 1] = 0.1  # SLAM says free
    anomalies = MapFusionProcess.detect_anomaly(osm, slam, threshold=0.7)
    types = [a["type"] for a in anomalies]
    assert "structure_changed" in types


def test_road_blocked_detection():
    osm, slam = make_layers()
    osm.grid[3, 3] = 0.2    # OSM says road
    slam.grid[3, 3] = 0.85  # SLAM says obstacle
    anomalies = MapFusionProcess.detect_anomaly(osm, slam, threshold=0.7)
    types = [a["type"] for a in anomalies]
    assert "road_blocked" in types


def test_threshold_boundary():
    osm, slam = make_layers()
    osm.grid[2, 2] = 0.1
    slam.grid[2, 2] = 0.69  # below threshold
    anomalies = MapFusionProcess.detect_anomaly(osm, slam, threshold=0.7)
    assert len(anomalies) == 0


def test_shape_mismatch_returns_empty():
    osm = OsmStaticLayer()
    osm.grid = np.zeros((3, 3), dtype=np.float32)
    slam = SlamPersistentLayer(grid_shape=(5, 5))
    anomalies = MapFusionProcess.detect_anomaly(osm, slam)
    assert anomalies == []
