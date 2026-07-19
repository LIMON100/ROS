"""Tests for OsmStaticLayer (P1-6, SDD Rev.A.6 §4.7.1)."""
import numpy as np
import pytest

from mapping.osm_static_layer import OsmStaticLayer


def test_init_with_default_cell():
    layer = OsmStaticLayer()
    assert layer.cell_size_m == 0.20


def test_get_cost_outside_grid_returns_unknown():
    layer = OsmStaticLayer()
    assert layer.get_cost(1.0, 1.0) == 0.5


def test_get_cost_inside_grid():
    layer = OsmStaticLayer()
    layer.grid = np.zeros((10, 10), dtype=np.float32)
    layer.grid[5, 5] = 1.0
    layer.utm_origin = (0.0, 0.0)
    cost = layer.get_cost(5 * 0.20 + 0.05, 5 * 0.20 + 0.05)
    assert cost == 1.0


def test_update_cell_clamps():
    layer = OsmStaticLayer()
    layer.grid = np.zeros((10, 10), dtype=np.float32)
    layer.utm_origin = (0.0, 0.0)
    layer.update_cell(0.05, 0.05, 1.5)   # > 1.0 → clamped to 1.0
    assert layer.get_cost(0.05, 0.05) == 1.0
    layer.update_cell(0.05, 0.05, -0.5)  # < 0.0 → clamped to 0.0
    assert layer.get_cost(0.05, 0.05) == 0.0


@pytest.mark.skip(reason="Requires real PBF + osmium - integration test")
def test_load_from_pbf_real():
    """Run with sample PBF in tests/data/sample_seoul.pbf"""
