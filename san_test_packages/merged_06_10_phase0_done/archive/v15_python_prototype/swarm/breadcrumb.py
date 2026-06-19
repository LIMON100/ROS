"""
BreadcrumbBuffer — leader-side trail of recent pose samples (SDD §6.7).

Followers in T4 (escape state, δ ≥ 2.0·d₀) follow this trail to rejoin the
formation when direct line-of-sight to the leader has been lost.

Sampling policy (per SDD):
  • Add a sample when distance from the previous sample ≥ 1.0 m,
    or time since the previous sample ≥ 0.5 s — whichever fires first.
  • Buffer caps: ≈ 1 200 samples / 1.2 km (10 min @ 2 m·s⁻¹ leader speed).
    Hard limit on count + soft limit on age (drops samples older than
    `max_age_s`).
  • Published on `sw_breadcrumb` (P0 RELIABLE) — DDS guarantees delivery
    in-order to subscribed followers.

Pure-Python implementation; tests construct it directly with no IPC.
"""
from __future__ import annotations

import math
from collections import deque
from typing import Iterable, Optional

from core.messages import BreadcrumbPoint


class BreadcrumbBuffer:
    """Sliding-window log of `BreadcrumbPoint`s.

    The buffer is intentionally NOT thread-safe — it's expected to live
    inside the leader's predictive-planner thread. Cross-thread access
    should go through that thread's lock.
    """

    DEFAULT_MIN_DIST_M = 1.0
    DEFAULT_MIN_INTERVAL_S = 0.5
    DEFAULT_MAX_POINTS = 1_200       # ≈ 1.2 km / 1 m
    DEFAULT_MAX_AGE_S = 600.0        # 10 min

    def __init__(
        self,
        min_dist_m: float = DEFAULT_MIN_DIST_M,
        min_interval_s: float = DEFAULT_MIN_INTERVAL_S,
        max_points: int = DEFAULT_MAX_POINTS,
        max_age_s: float = DEFAULT_MAX_AGE_S,
    ):
        self.min_dist_m = float(min_dist_m)
        self.min_interval_s = float(min_interval_s)
        self.max_points = int(max_points)
        self.max_age_s = float(max_age_s)
        self._buf: deque[BreadcrumbPoint] = deque(maxlen=max_points)
        self._seq: int = 0

    def __len__(self) -> int:
        return len(self._buf)

    def offer(self, *, x: float, y: float, yaw: float,
              stamp: float) -> Optional[BreadcrumbPoint]:
        """Submit a candidate sample. Returns the newly-recorded point if
        the policy accepted it, else None.

        Acceptance rule: first sample is always accepted; subsequent ones
        only when displacement ≥ min_dist_m OR elapsed ≥ min_interval_s.
        """
        if self._buf:
            last = self._buf[-1]
            dx, dy = x - last.x, y - last.y
            dist = math.hypot(dx, dy)
            elapsed = stamp - last.stamp
            if dist < self.min_dist_m and elapsed < self.min_interval_s:
                return None
        pt = BreadcrumbPoint(seq=self._seq, stamp=stamp, x=x, y=y, yaw=yaw)
        self._seq += 1
        self._buf.append(pt)
        self._evict_old(now=stamp)
        return pt

    def _evict_old(self, *, now: float) -> None:
        """Drop samples older than max_age_s. Cheap because the buffer is
        time-monotonic — we just pop from the left."""
        cutoff = now - self.max_age_s
        while self._buf and self._buf[0].stamp < cutoff:
            self._buf.popleft()

    def latest(self) -> Optional[BreadcrumbPoint]:
        return self._buf[-1] if self._buf else None

    def snapshot(self) -> list[BreadcrumbPoint]:
        return list(self._buf)

    def replay_from(self, *, x: float, y: float) -> Iterable[BreadcrumbPoint]:
        """Return the trail starting from the buffer point closest to
        (x, y) — used by a follower in T4 to find its insertion point.

        Iterates from there forward toward the latest point, so the
        follower's planner can drive forward along the breadcrumb in
        time order.
        """
        snap = self.snapshot()
        if not snap:
            return []
        best_i = 0
        best_d = math.hypot(snap[0].x - x, snap[0].y - y)
        for i, p in enumerate(snap[1:], start=1):
            d = math.hypot(p.x - x, p.y - y)
            if d < best_d:
                best_d, best_i = d, i
        return snap[best_i:]
