"""
Patrol planning: load patrol routes from YAML, manage schedule.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import List, Optional

import numpy as np

from core.messages import Header, Pose6D, Waypoint

try:
    import yaml
except ImportError:
    yaml = None


@dataclass
class PatrolRoute:
    name: str
    floor: int
    schedule: List[str]                # "06:00" etc.
    waypoints: List[Waypoint] = field(default_factory=list)


class PatrolPlanner:
    def __init__(self, routes_file: str):
        self.routes_file = Path(routes_file)
        self.routes: List[PatrolRoute] = []
        self._last_triggered: dict = {}

    def load(self) -> None:
        if not yaml or not self.routes_file.exists():
            self.routes = [self._default_route()]
            return
        with open(self.routes_file) as f:
            data = yaml.safe_load(f) or {}
        self.routes = []
        for r in data.get("routes", []):
            wps = []
            for w in r.get("waypoints", []):
                pos = np.array(w["pose"]["position"], dtype=np.float32)
                quat = np.array(w["pose"].get("orientation", [0, 0, 0, 1]),
                                dtype=np.float32)
                wps.append(Waypoint(
                    id=w["id"],
                    pose=Pose6D(header=Header(), position=pos, orientation=quat),
                    dwell_sec=float(w.get("dwell_sec", 30.0)),
                    checks=tuple(w.get("checks", [])),
                ))
            self.routes.append(PatrolRoute(
                name=r["name"], floor=int(r.get("floor", 1)),
                schedule=r.get("schedule", []), waypoints=wps,
            ))

    def due_route(self, now: Optional[datetime] = None) -> Optional[PatrolRoute]:
        """Return a route whose schedule fires now (within ±60s window)."""
        now = now or datetime.now()
        for r in self.routes:
            for sch in r.schedule:
                hh, mm = sch.split(":")
                target = now.replace(hour=int(hh), minute=int(mm),
                                     second=0, microsecond=0)
                key = (r.name, sch, target.date())
                if key in self._last_triggered:
                    continue
                if 0 <= (now - target).total_seconds() < 60:
                    self._last_triggered[key] = now
                    return r
        return None

    @staticmethod
    def _default_route() -> PatrolRoute:
        wps = [
            Waypoint(id=f"P{i}",
                     pose=Pose6D(header=Header(),
                                 position=np.array([5*i, 2.0, 0], dtype=np.float32),
                                 orientation=np.array([0, 0, 0, 1], dtype=np.float32)),
                     dwell_sec=30.0,
                     checks=("ppe", "hazard"))
            for i in range(1, 5)
        ]
        return PatrolRoute(name="default", floor=1,
                           schedule=["06:00", "10:00", "14:00", "18:00"],
                           waypoints=wps)
