"""SAN v1.3 §9 SLAM cadence invariants (PHASE 3).

Pins the v1.3 cadence numbers so a future tuning PR can't silently
revert to v1.1 (30 s) defaults. Also asserts the publisher and
aggregator stay locked to the same table.
"""
from __future__ import annotations

import pytest

from mapping.aggregated_map import (
    DEFAULT_PERIOD_S,
    LEGACY_DEFAULT_PERIOD_S,
    MAX_PERIOD_S,
    MIN_PERIOD_S,
    AggregatedMapDispatcher,
)
from mapping.slam_aggregator import PERIOD_BY_MODE as AGG_TABLE
from mapping.slam_local_publisher import PERIOD_BY_MODE as PUB_TABLE


def test_v13_default_period_is_5s():
    assert DEFAULT_PERIOD_S == 5.0


def test_v11_legacy_period_preserved_for_back_compat():
    """v1.1 baseline (30 s) is kept as a named constant so callers /
    dashboards / replay tools that explicitly want the legacy pace can
    opt in without hard-coding a number.
    """
    assert LEGACY_DEFAULT_PERIOD_S == 30.0


def test_period_clamp_window_relaxed_to_1_60s():
    assert MIN_PERIOD_S == 1.0
    assert MAX_PERIOD_S == 60.0


def test_dispatcher_no_longer_clamps_5s_up():
    """Regression guard: the v1.1 dispatcher clamped period < 30 s up
    to 30 s. v1.3 must accept 5 s as-is so the production default
    actually fires every 5 s.
    """
    d = AggregatedMapDispatcher(period_s=5.0)
    assert d.period_s == 5.0


def test_publisher_aggregator_table_locked():
    """The follower publisher and the Hub aggregator must publish on
    the same cadence — otherwise the Hub fuses across mismatched
    windows. Defensive test: any mode added on one side must appear
    on the other.
    """
    assert set(AGG_TABLE) == set(PUB_TABLE)
    for mode in AGG_TABLE:
        assert AGG_TABLE[mode] == PUB_TABLE[mode], (
            f"mode={mode}: aggregator={AGG_TABLE[mode]} != "
            f"publisher={PUB_TABLE[mode]}")


@pytest.mark.parametrize("mode,expected", [
    ("wide",     30.0),
    ("default",   5.0),
    ("narrow",    2.5),
    ("obstacle",  1.0),
])
def test_v13_mode_period_values(mode, expected):
    assert AGG_TABLE[mode] == expected
