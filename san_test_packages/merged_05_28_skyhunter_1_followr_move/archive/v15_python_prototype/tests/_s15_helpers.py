"""Shared helpers for the S15 scenario tests.

`compute_total_coverage_deg` is the workhorse: given a list of
(start_deg, end_deg) sectors using the project's CCW convention
(see swarm/sector_assign.py), it returns the total angular extent
of the union (in degrees, ≤ 360). Wrap-around sectors — the kind
the Hub UGV uses for the rear arc — are split across ±180° and
merged correctly.

This is exercise-only code; not imported by production modules.
"""
from __future__ import annotations

from typing import Iterable, List, Sequence, Tuple

from swarm.sector_assign import sector_width_deg


def _split_sector_into_intervals(
    start_deg: float, end_deg: float,
) -> List[Tuple[float, float]]:
    """Convert one signed-degree sector into one or two 0–360 intervals.

    Returns [(a, b), ...] where each interval covers `b - a` degrees on
    the [0, 360) circle. A wrapping sector (e.g. Hub's +150°→-150°)
    yields TWO intervals — [150, 360] + [0, 210] — so a downstream
    union-merge sees them as a single 60° arc straddling ±180.
    """
    width = sector_width_deg(start_deg, end_deg)
    if width <= 0:
        return []
    if width >= 360.0:
        return [(0.0, 360.0)]
    start_norm = start_deg % 360.0
    end_pos = start_norm + width
    if end_pos <= 360.0 + 1e-9:
        return [(start_norm, min(end_pos, 360.0))]
    # Wraps through 360 — split.
    return [(start_norm, 360.0), (0.0, end_pos - 360.0)]


def compute_total_coverage_deg(
    sectors: Iterable[Tuple[float, float]],
) -> float:
    """Total angular coverage of the union of N sectors, in degrees.

    Implementation: convert each sector to one or two intervals on
    [0, 360); sort by left edge; merge overlapping intervals; sum
    lengths. Returns a value in [0, 360]; 360 means "full circle."
    """
    intervals: List[Tuple[float, float]] = []
    for s, e in sectors:
        intervals.extend(_split_sector_into_intervals(s, e))
    if not intervals:
        return 0.0
    intervals.sort(key=lambda p: p[0])
    merged: List[List[float]] = [list(intervals[0])]
    for a, b in intervals[1:]:
        prev = merged[-1]
        if a <= prev[1] + 1e-9:
            prev[1] = max(prev[1], b)
        else:
            merged.append([a, b])
    total = sum(b - a for a, b in merged)
    return min(total, 360.0)


def sectors_from_plans(plans: Sequence) -> List[Tuple[float, float]]:
    """Convenience: pluck (start, end) tuples from a list of SectorPlan
    or SectorAssign objects.
    """
    return [(p.sector_start_deg, p.sector_end_deg) for p in plans]


__all__ = (
    "compute_total_coverage_deg",
    "sectors_from_plans",
)
