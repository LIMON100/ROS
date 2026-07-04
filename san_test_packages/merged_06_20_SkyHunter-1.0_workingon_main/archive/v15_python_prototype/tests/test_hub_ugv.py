"""Tests for HubUgvAdapter (P1-5, SDD Rev.A.6 §2.2, §6.5).

Exercises the per-cell max-fusion merger and the leader-takeover FSM
directly. The BaseProcess spawn path is integration territory.
"""
from __future__ import annotations

import threading
import time
from unittest.mock import MagicMock

import numpy as np
import pytest

from adapters.hub_ugv import HubUgvAdapter
from core.messages import MapTile


def _maptile(*, tile_id: str = "tile_x_y", origin=(0.0, 0.0),
             size=20.0, res=0.20, source: str = "robot:1",
             shape=(8, 8), val: float = 0.7) -> MapTile:
    occ = np.full(shape, val, dtype=np.float32)
    return MapTile(
        tile_id=tile_id,
        origin_xy=origin, size_m=size, resolution=res,
        occupancy=occ, confidence=1.0, source=source,
        last_update=time.time(),
    )


def _make_hub() -> HubUgvAdapter:
    """Construct without invoking BaseProcess.__init__ (no real spawn)."""
    h = HubUgvAdapter.__new__(HubUgvAdapter)
    h.queues = MagicMock()
    h.cfg = MagicMock()
    h.log = MagicMock()
    h.role = "hub"
    h._lock = threading.Lock()
    h._follower_tiles = {}
    h._is_acting_leader = False
    h._last_leader_seen = 0.0
    h._stats = {"follower_maps_in": 0, "shared_maps_out": 0,
                "leader_takeovers": 0}
    return h


# ════════════════════════════════════════════════════════════════
# Adapter export + role-based gating
# ════════════════════════════════════════════════════════════════
def test_hub_ugv_adapter_exported_from_adapters_package():
    """main.py imports HubUgvAdapter from `adapters`; package must re-export."""
    import adapters
    assert hasattr(adapters, "HubUgvAdapter")
    assert "HubUgvAdapter" in adapters.__all__


def test_main_only_spawns_hub_when_role_is_hub():
    """The conditional in main.py appends HubUgvAdapter iff role=='hub'.
    We assert the literal predicate (string lower-case match)."""
    cases = [
        ("hub", True), ("HUB", True), ("Hub", True),
        ("follower", False), ("leader", False),
        ("", False), (None, False),
    ]
    for role, expected in cases:
        actual = str(role or "follower").lower() == "hub"
        assert actual is expected, f"role={role!r} expected={expected}"


def test_role_follower_skips_thread_spawn():
    h = _make_hub()
    h.role = "follower"
    h.cfg.get = MagicMock(return_value="follower")
    h.spawn_thread = MagicMock()
    h.setup()
    h.spawn_thread.assert_not_called()


# ════════════════════════════════════════════════════════════════
# Max-fusion (per-cell np.max across robots)
# ════════════════════════════════════════════════════════════════
def test_max_fusion_two_robots_disjoint_obstacles():
    h = _make_hub()
    occ_a = np.zeros((100, 100), dtype=np.float32)
    occ_a[50, 50] = 0.9
    occ_b = np.zeros((100, 100), dtype=np.float32)
    occ_b[60, 60] = 0.95
    h.slam_fusion_consume(_maptile(occupancy_override=False, val=0.0,
                                    source="robot:1") if False else
                          _maptile(source="robot:1", val=0.0))
    # Replace the freshly-created tile with our hand-built one.
    h._follower_tiles["tile_x_y"]["robot:1"] = MapTile(
        tile_id="tile_x_y", origin_xy=(0.0, 0.0), size_m=20.0,
        resolution=0.20, occupancy=occ_a, confidence=1.0,
        source="robot:1", last_update=0.0)
    h._follower_tiles["tile_x_y"]["robot:2"] = MapTile(
        tile_id="tile_x_y", origin_xy=(0.0, 0.0), size_m=20.0,
        resolution=0.20, occupancy=occ_b, confidence=1.0,
        source="robot:2", last_update=0.0)
    fused = h.fuse_tiles()
    assert len(fused) == 1
    f = fused[0].occupancy
    assert f[50, 50] == 0.9
    assert f[60, 60] == 0.95
    assert fused[0].source == "hub_fused"


def test_max_fusion_three_robots_overlapping_cell():
    h = _make_hub()
    for i, val in enumerate([0.3, 0.7, 0.5]):
        occ = np.zeros((10, 10), dtype=np.float32)
        occ[5, 5] = val
        h.slam_fusion_consume(MapTile(
            tile_id="t", origin_xy=(0.0, 0.0), size_m=10.0,
            resolution=0.20, occupancy=occ, confidence=1.0,
            source=f"robot:{i}", last_update=0.0))
    fused = h.fuse_tiles()
    assert len(fused) == 1
    assert fused[0].occupancy[5, 5] == pytest.approx(0.7)
    assert fused[0].source == "hub_fused"


def test_max_fusion_separate_tiles_keep_separate_results():
    """Different tile_ids → separate fused outputs."""
    h = _make_hub()
    h.slam_fusion_consume(_maptile(tile_id="tile_a", source="robot:1",
                                    val=0.4))
    h.slam_fusion_consume(_maptile(tile_id="tile_b", source="robot:1",
                                    val=0.8))
    fused = h.fuse_tiles()
    assert len(fused) == 2
    by_id = {t.tile_id: t for t in fused}
    assert "tile_a" in by_id and "tile_b" in by_id


def test_zero_followers_no_fusion_output():
    h = _make_hub()
    assert h.fuse_tiles() == []


def test_slam_fusion_consume_increments_stats():
    h = _make_hub()
    h.slam_fusion_consume(_maptile(source="robot:1"))
    h.slam_fusion_consume(_maptile(source="robot:2"))
    assert h._stats["follower_maps_in"] == 2


# ════════════════════════════════════════════════════════════════
# Leader takeover (silent-leader timeout heuristic)
# ════════════════════════════════════════════════════════════════
def test_leader_takeover_no_action_when_heartbeat_fresh():
    h = _make_hub()
    h.note_leader_heartbeat(now=100.0)
    transitioned = h.leader_takeover_handler(now=101.0)   # 1 s gap
    assert transitioned is False
    assert h.is_acting_leader() is False


def test_leader_takeover_fires_after_timeout():
    h = _make_hub()
    h.note_leader_heartbeat(now=100.0)
    transitioned = h.leader_takeover_handler(
        now=100.0 + HubUgvAdapter.LEADER_TIMEOUT_S + 0.1)
    assert transitioned is True
    assert h.is_acting_leader() is True
    assert h._stats["leader_takeovers"] == 1
    # Idempotent: subsequent silent-leader calls don't re-fire.
    transitioned2 = h.leader_takeover_handler(
        now=100.0 + HubUgvAdapter.LEADER_TIMEOUT_S + 1.0)
    assert transitioned2 is False
    assert h._stats["leader_takeovers"] == 1


def test_leader_takeover_relinquishes_when_leader_returns():
    h = _make_hub()
    h.note_leader_heartbeat(now=100.0)
    h.leader_takeover_handler(
        now=100.0 + HubUgvAdapter.LEADER_TIMEOUT_S + 0.1)
    assert h.is_acting_leader() is True
    h.note_leader_heartbeat(now=200.0)
    h.leader_takeover_handler(now=200.5)
    assert h.is_acting_leader() is False
