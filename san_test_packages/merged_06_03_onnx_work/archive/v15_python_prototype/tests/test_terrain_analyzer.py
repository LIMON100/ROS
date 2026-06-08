"""Tests for auto terrain switch (P2-3, SDD §7.5)."""
from __future__ import annotations

import time

from mission.terrain_analyzer import TerrainAnalyzer, TerrainState
from swarm.tier_manager import Tier


def test_initial_state_is_open():
    a = TerrainAnalyzer()
    assert a.state == TerrainState.OPEN


def test_open_terrain_no_triggers():
    a = TerrainAnalyzer()
    ev = a.evaluate(
        lidar_passable_width_m=20.0,
        costmap_lateral_costs=(0.1, 0.1),
        dem_elevation_diff_m=0.2,
        follower_tiers=[Tier.T0] * 5,
        current_formation_d_m=5.0,
    )
    assert ev.state == TerrainState.OPEN
    assert ev.triggers.any_triggered() is False


def test_lidar_narrow_triggers_pending():
    a = TerrainAnalyzer()
    ev = a.evaluate(
        lidar_passable_width_m=5.0,  # narrow vs d_across=7.5+2=9.5
        costmap_lateral_costs=(0.1, 0.1),
        dem_elevation_diff_m=0.2,
        follower_tiers=[Tier.T0] * 5,
        current_formation_d_m=5.0,
    )
    assert ev.state == TerrainState.NARROW_PENDING
    assert ev.triggers.lidar_narrow is True


def test_costmap_blocked_lateral():
    a = TerrainAnalyzer()
    ev = a.evaluate(
        lidar_passable_width_m=20.0,
        costmap_lateral_costs=(0.99, 0.1),
        dem_elevation_diff_m=0.2,
        follower_tiers=[Tier.T0] * 5,
    )
    assert ev.triggers.costmap_blocked_lateral is True
    assert ev.state == TerrainState.NARROW_PENDING


def test_dem_elevation_change_triggers():
    a = TerrainAnalyzer()
    ev = a.evaluate(
        lidar_passable_width_m=20.0,
        costmap_lateral_costs=(0.1, 0.1),
        dem_elevation_diff_m=1.5,
        follower_tiers=[Tier.T0] * 5,
    )
    assert ev.triggers.dem_elevation_change is True


def test_tier4_ratio_triggers():
    a = TerrainAnalyzer()
    tiers = [Tier.T4, Tier.T4, Tier.T0, Tier.T0, Tier.T0]
    ev = a.evaluate(
        lidar_passable_width_m=20.0,
        costmap_lateral_costs=(0.1, 0.1),
        dem_elevation_diff_m=0.2,
        follower_tiers=tiers,
    )
    assert ev.triggers.tier4_ratio is True


def test_tier4_ratio_accepts_string_tiers():
    """Tier values forwarded over the sw_tier DDS topic arrive as
    strings ("T4"), not the IntEnum. The trigger must still fire."""
    a = TerrainAnalyzer()
    tiers = ["T4", "T4", "T0", "T0", "T0"]  # 2/5 = 40% > 33%
    ev = a.evaluate(
        lidar_passable_width_m=20.0,
        costmap_lateral_costs=(0.1, 0.1),
        dem_elevation_diff_m=0.2,
        follower_tiers=tiers,
    )
    assert ev.triggers.tier4_ratio is True


def test_pending_to_open_if_clears_during_5s():
    a = TerrainAnalyzer()
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.NARROW_PENDING
    a.evaluate(lidar_passable_width_m=20.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.OPEN


def test_pending_advances_to_active_after_5s():
    a = TerrainAnalyzer()
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    a._pending_since = time.monotonic() - 6.0
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.NARROW_ACTIVE


def test_manual_override_accept_immediately_active():
    a = TerrainAnalyzer()
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.NARROW_PENDING
    a.manual_override(accept=True)
    assert a.state == TerrainState.NARROW_ACTIVE


def test_manual_override_cancel_back_to_open():
    a = TerrainAnalyzer()
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    a.manual_override(accept=False)
    assert a.state == TerrainState.OPEN


def test_revert_after_10s_hysteresis():
    a = TerrainAnalyzer()
    a.state = TerrainState.NARROW_ACTIVE
    a.evaluate(lidar_passable_width_m=20.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.CLEARING
    a._clearing_since = time.monotonic() - 11.0
    a.evaluate(lidar_passable_width_m=20.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.OPEN


def test_clearing_reverts_to_active_if_trigger_fires_again():
    a = TerrainAnalyzer()
    a.state = TerrainState.NARROW_ACTIVE
    a.evaluate(lidar_passable_width_m=20.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.CLEARING
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert a.state == TerrainState.NARROW_ACTIVE


def test_callback_invoked_on_active():
    a = TerrainAnalyzer()
    fired: list = []
    a.set_auto_switch_callback(lambda b: fired.append(b))
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    a._pending_since = time.monotonic() - 6.0
    a.evaluate(lidar_passable_width_m=5.0,
               costmap_lateral_costs=(0.1, 0.1),
               dem_elevation_diff_m=0.2,
               follower_tiers=[Tier.T0] * 5)
    assert len(fired) == 1
    assert fired[0] is True
