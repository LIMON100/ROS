"""Surveillance sector assignment — leader → robot.

Topic: /swarm/surveillance/sector_assign  (QoS P1 RELIABLE)
Cadence: 10 s periodic + event (sector change, roster change).

The leader's MissionProcess owns a SectorAssignDispatcher; each tick it
calls `due_messages(now_ms, roster)` and publishes whatever is returned
on `queues.sw_sector_assign`. Followers run `filter_for_robot` on each
incoming message and cache only their own assignment.

v1.5 references (DCN-2026-001 D-001):
  * SDD-SWARM v1.5 §1.2.1 — 8-robot squadron (Leader + Hub + Deputy + 1~5 Followers)
  * SDD-SUR v1.5 §3.2     — Deputy UGV shadow coverage (Hub rear-180° phase-offset sweep)
  * IDS  v1.5 §5          — RobotStatus.is_deputy_ugv (S3 identification flag)
"""
from __future__ import annotations

import dataclasses
from dataclasses import dataclass
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

from core.messages import (
    SECTOR_MODE_SWEEP,
    SECTOR_MODE_TRACK,
    SECTOR_PRIORITY_OVERLAP,
    SECTOR_PRIORITY_PRIMARY,
    SECTOR_PRIORITY_SHADOW,
    SECTOR_PRIORITY_THREAT_FOCUS,
    SectorAssign,
)

PERIOD_MS = 10_000   # 10 s P1 cadence

# v1.5 (SDD-SUR §3.2): Deputy UGV sweeps the Hub's rear sector with a
# 180° phase offset to double the revisit rate over the rear 180° band.
DEPUTY_PHASE_OFFSET_DEG = 180.0


@dataclass
class SectorPlan:
    """Per-robot sector + metadata as decided by the assignment policy.

    v1.5: phase_offset_deg is non-zero only for SHADOW-priority slots
    (Deputy UGV shadowing the Hub). Other priorities use 0.0.
    """
    robot_id: int
    sector_start_deg: float
    sector_end_deg: float
    priority: str = SECTOR_PRIORITY_PRIMARY
    mode_hint: str = SECTOR_MODE_SWEEP
    phase_offset_deg: float = 0.0


def equal_split(
    robot_ids: Sequence[int],
    coverage_deg: float = 360.0,
    center_deg: float = 0.0,
) -> List[SectorPlan]:
    """Divide `coverage_deg` evenly across robots, centered on `center_deg`.

    With the default 360°, robot 0 gets the rear-quadrant slice and the
    final robot gets the slice just before it — sectors wrap around the
    full body-frame circle. With a narrower coverage (e.g. 180°), the
    slices fill only the forward arc.
    """
    if not robot_ids:
        return []
    n = len(robot_ids)
    width = coverage_deg / n
    start_of_arc = center_deg - coverage_deg / 2.0
    plans: List[SectorPlan] = []
    for i, rid in enumerate(robot_ids):
        s = start_of_arc + i * width
        e = s + width
        # Wrap into [-180, 180] without losing the sweep direction.
        plans.append(SectorPlan(
            robot_id=int(rid),
            sector_start_deg=_wrap_deg(s),
            sector_end_deg=_wrap_deg(e),
        ))
    return plans


def _wrap_deg(d: float) -> float:
    while d > 180.0:
        d -= 360.0
    while d < -180.0:
        d += 360.0
    return d


# ─── V-formation policy ──────────────────────────────────────────────────
# Per the v1.5 spec, the standard 8-slot V-formation map is anchored on
# the four named roles (Leader, Hub, Deputy, F1..F5) plus the v1.3-era
# legacy 6-follower geometry that the surveillance dispatcher still
# inherits. Body-frame angles, +X = forward:
#   0° = forward, +90° = robot's left, ±180° = rear.
# Each value is (sector_start_deg, sector_end_deg, priority). The Hub slot
# wraps through ±180° (+150° → -150° CCW) so the Hub UGV covers the rear
# 60° "principal" band. v1.5 adds a Deputy slot covering the same band
# with SHADOW priority — the runtime assigns a 180° phase offset so the
# Hub and Deputy sweep in opposition, doubling rear revisit rate.
STANDARD_V_FORMATION_ROLES: Dict[str, Tuple[float, float, str]] = {
    "leader":    ( -30.0,  +30.0, SECTOR_PRIORITY_PRIMARY),
    "follower1": ( -90.0,  -30.0, SECTOR_PRIORITY_PRIMARY),
    "follower2": ( +30.0,  +90.0, SECTOR_PRIORITY_PRIMARY),
    "follower3": (-120.0,  -60.0, SECTOR_PRIORITY_OVERLAP),
    "follower4": ( +60.0, +120.0, SECTOR_PRIORITY_OVERLAP),
    "follower5": (-150.0,  -90.0, SECTOR_PRIORITY_OVERLAP),
    "follower6": ( +90.0, +150.0, SECTOR_PRIORITY_OVERLAP),
    "hub":       (+150.0, -150.0, SECTOR_PRIORITY_PRIMARY),
    # v1.5 (DCN-2026-001 D-001, SDD-SUR §3.2): Deputy UGV (S3) shadow
    # coverage. Same sector as Hub, but with phase-offset sweep.
    "deputy":    (+150.0, -150.0, SECTOR_PRIORITY_SHADOW),
}

# Modes the V-formation dispatcher knows how to bias. "default" is the
# balanced V; "engage" tightens the forward primary sectors to 30°
# centered on their original midpoint; "defensive" is a no-op alias.
V_FORMATION_MODE_DEFAULT   = "default"
V_FORMATION_MODE_ENGAGE    = "engage"
V_FORMATION_MODE_DEFENSIVE = "defensive"
V_FORMATION_MODES = (
    V_FORMATION_MODE_DEFAULT,
    V_FORMATION_MODE_ENGAGE,
    V_FORMATION_MODE_DEFENSIVE,
)


def sector_width_deg(start_deg: float, end_deg: float) -> float:
    """CCW width of a sector. Handles wrap-around (start > end).

    A sector with start == end returns 0° (not 360°); callers that mean
    "full circle" should encode it explicitly rather than relying on
    degenerate endpoints.
    """
    w = (end_deg - start_deg) % 360.0
    # A literal full-circle (s == e) collapses to 0 — that's what we want
    # so a degenerate plan doesn't masquerade as global coverage.
    return float(w)


def sector_center_deg(start_deg: float, end_deg: float) -> float:
    """CCW midpoint of a sector, wrapped into [-180, 180]."""
    w = sector_width_deg(start_deg, end_deg)
    return _wrap_deg(start_deg + w / 2.0)


def _sector_contains(
    outer_start: float, outer_end: float,
    inner_start: float, inner_end: float,
    eps: float = 1e-6,
) -> bool:
    """True if the CCW arc [inner_start, inner_end] lies inside [outer_start, outer_end]."""
    outer_w = sector_width_deg(outer_start, outer_end)
    inner_w = sector_width_deg(inner_start, inner_end)
    offset  = sector_width_deg(outer_start, inner_start)
    return offset + inner_w <= outer_w + eps


def union_sectors(
    s1: float, e1: float, s2: float, e2: float,
) -> Optional[Tuple[float, float]]:
    """Smallest CCW arc containing both sectors, or None if they're disjoint
    with no candidate hull that wraps tighter than 360°.

    Picks among the 4 candidate (start, end) pairs whose endpoints come
    from {s1, e1, s2, e2}. Wrap-aware: F6 (+90, +150) ∪ Hub (+150, -150)
    returns (+90, -150) — i.e. a 120° CCW arc that crosses ±180°.
    """
    candidates = []
    for start in (s1, s2):
        for end in (e1, e2):
            if (_sector_contains(start, end, s1, e1)
                    and _sector_contains(start, end, s2, e2)):
                candidates.append((start, end))
    if not candidates:
        return None
    return min(candidates, key=lambda se: sector_width_deg(*se))


def _angular_distance_deg(a: float, b: float) -> float:
    """Shortest signed distance between two headings, magnitude in [0, 180]."""
    d = (a - b + 180.0) % 360.0 - 180.0
    return abs(d)


def v_formation_sectors(
    role_to_robot_id: Mapping[str, int],
    alive_robot_ids: Iterable[int],
    roles: Mapping[str, Tuple[float, float, str]] = STANDARD_V_FORMATION_ROLES,
) -> List[SectorPlan]:
    """Map standard roles to alive robots; one SectorPlan per assigned role.

    Roles whose robot is not in `alive_robot_ids` (or has no mapping) are
    skipped. The output is *unfilled* — callers wanting gap coverage when
    a role drops out should chain `apply_gap_fill`.

    v1.5: SHADOW-priority roles (currently Deputy UGV) receive
    `phase_offset_deg = DEPUTY_PHASE_OFFSET_DEG` (180°) automatically; all
    other priorities use 0.0. Callers may override later if needed.
    """
    alive = set(int(r) for r in alive_robot_ids)
    plans: List[SectorPlan] = []
    for role, (start, end, prio) in roles.items():
        rid = role_to_robot_id.get(role)
        if rid is None or int(rid) not in alive:
            continue
        offset = (
            DEPUTY_PHASE_OFFSET_DEG
            if prio == SECTOR_PRIORITY_SHADOW
            else 0.0
        )
        plans.append(SectorPlan(
            robot_id=int(rid),
            sector_start_deg=float(start),
            sector_end_deg=float(end),
            priority=prio,
            mode_hint=SECTOR_MODE_SWEEP,
            phase_offset_deg=offset,
        ))
    return plans


def apply_gap_fill(
    plans: Sequence[SectorPlan],
    role_to_robot_id: Mapping[str, int],
    roles: Mapping[str, Tuple[float, float, str]] = STANDARD_V_FORMATION_ROLES,
) -> List[SectorPlan]:
    """Widen present sectors to cover roles whose robots dropped out.

    For each role that has no plan, the alive role whose sector center is
    nearest (ties broken alphabetically by role name for determinism) is
    extended to the union of its sector and the missing role's sector.
    The widened plan's priority becomes OVERLAP to signal it is doing
    double duty.
    """
    if not plans:
        return []
    plans_by_id: Dict[int, SectorPlan] = {p.robot_id: p for p in plans}
    # role → currently-assigned robot_id (and whether that plan exists).
    role_by_robot_id = {int(rid): role for role, rid in role_to_robot_id.items()}
    present_roles = {
        role_by_robot_id[rid]
        for rid in plans_by_id
        if rid in role_by_robot_id and role_by_robot_id[rid] in roles
    }
    # v1.5: a missing SHADOW slot (e.g. Deputy down) does NOT trigger gap
    # fill — its principal (Hub) is already covering the same sector at
    # PRIMARY priority. Limp Mode escalation is the right response when
    # Hub + Deputy both drop, handled by LimpModeManager.
    missing_roles = [
        r for r in roles
        if r not in present_roles and roles[r][2] != SECTOR_PRIORITY_SHADOW
    ]
    if not missing_roles:
        return [dataclasses.replace(p) for p in plans]

    out: Dict[int, SectorPlan] = {
        rid: dataclasses.replace(p) for rid, p in plans_by_id.items()
    }
    for m_role in missing_roles:
        m_start, m_end, _m_prio = roles[m_role]
        m_center = sector_center_deg(m_start, m_end)
        # Nearest present role by sector center; alphabetical role-name
        # tiebreak keeps the result deterministic when two neighbours are
        # equidistant (e.g. leader missing → F1 vs F2 are both 60° away).
        # v1.5: SHADOW slots are excluded from widening candidates too —
        # the Deputy's job is the rear phase-offset sweep, not gap fill.
        present_role_names = sorted(
            r for r in present_roles
            if roles.get(r, (0, 0, ""))[2] != SECTOR_PRIORITY_SHADOW
        )
        if not present_role_names:
            continue
        best_role = min(
            present_role_names,
            key=lambda r: (
                _angular_distance_deg(
                    sector_center_deg(*roles[r][:2]), m_center),
                r,
            ),
        )
        best_rid = int(role_to_robot_id[best_role])
        bp = out[best_rid]
        merged = union_sectors(
            bp.sector_start_deg, bp.sector_end_deg, m_start, m_end)
        if merged is None:
            # Disjoint with no smaller-than-circle hull — leave bp as-is.
            continue
        new_start, new_end = merged
        out[best_rid] = dataclasses.replace(
            bp,
            sector_start_deg=float(new_start),
            sector_end_deg=float(new_end),
            priority=SECTOR_PRIORITY_OVERLAP,
        )
    return list(out.values())


def apply_threat_focus(
    plans: Sequence[SectorPlan],
    threat_bearings_deg: Iterable[float],
    focus_half_width_deg: float = 15.0,
) -> List[SectorPlan]:
    """Retarget the nearest alive sector(s) to focus on each threat bearing.

    For every bearing, the plan whose sector center is closest is rewritten
    to a `focus_half_width_deg`-wide window centered on the threat with
    priority=threat_focus and mode_hint=track. The second-closest plan
    (if any) is bumped to overlap+track as a backup tracker — but only
    if it wasn't already tasked by an earlier threat in this call.
    """
    bearings = [float(b) for b in threat_bearings_deg]
    if not bearings or not plans:
        return [dataclasses.replace(p) for p in plans]

    out: List[SectorPlan] = [dataclasses.replace(p) for p in plans]
    taken: set[int] = set()
    half = float(focus_half_width_deg)
    for b in bearings:
        ranked = sorted(
            range(len(out)),
            key=lambda i: _angular_distance_deg(
                sector_center_deg(out[i].sector_start_deg,
                                  out[i].sector_end_deg),
                b),
        )
        primary_i = next((i for i in ranked if i not in taken), None)
        if primary_i is None:
            continue
        taken.add(primary_i)
        out[primary_i] = dataclasses.replace(
            out[primary_i],
            sector_start_deg=_wrap_deg(b - half),
            sector_end_deg=_wrap_deg(b + half),
            priority=SECTOR_PRIORITY_THREAT_FOCUS,
            mode_hint=SECTOR_MODE_TRACK,
        )
        backup_i = next((i for i in ranked if i not in taken), None)
        if backup_i is not None:
            taken.add(backup_i)
            out[backup_i] = dataclasses.replace(
                out[backup_i],
                priority=SECTOR_PRIORITY_OVERLAP,
                mode_hint=SECTOR_MODE_TRACK,
            )
    return out


def apply_mode_adjustment(
    plans: Sequence[SectorPlan],
    mode: str,
    role_to_robot_id: Optional[Mapping[str, int]] = None,
) -> List[SectorPlan]:
    """Tighten or balance sectors based on V-formation operating mode.

    `engage` halves the forward primary sectors (leader, F1, F2) around
    their current center — concentrating optical coverage where contact
    is expected. `defensive` and `default` are no-ops; the balanced V is
    already symmetric.
    """
    if mode not in V_FORMATION_MODES:
        raise ValueError(f"unknown V-formation mode: {mode!r}")
    if mode != V_FORMATION_MODE_ENGAGE or role_to_robot_id is None:
        return [dataclasses.replace(p) for p in plans]
    forward_roles = ("leader", "follower1", "follower2")
    forward_ids = {
        int(role_to_robot_id[r]) for r in forward_roles
        if r in role_to_robot_id
    }
    out: List[SectorPlan] = []
    for p in plans:
        if p.robot_id not in forward_ids:
            out.append(dataclasses.replace(p))
            continue
        c = sector_center_deg(p.sector_start_deg, p.sector_end_deg)
        w = sector_width_deg(p.sector_start_deg, p.sector_end_deg) / 2.0
        out.append(dataclasses.replace(
            p,
            sector_start_deg=_wrap_deg(c - w / 2.0),
            sector_end_deg=_wrap_deg(c + w / 2.0),
        ))
    return out


def compute_v_formation_sectors(
    role_to_robot_id: Mapping[str, int],
    alive_robot_ids: Iterable[int],
    threat_bearings_deg: Iterable[float] = (),
    mode: str = V_FORMATION_MODE_DEFAULT,
    roles: Mapping[str, Tuple[float, float, str]] = STANDARD_V_FORMATION_ROLES,
    focus_half_width_deg: float = 15.0,
) -> List[SectorPlan]:
    """V-formation pipeline: base → gap-fill → threat-focus → mode adjust.

    Threat focus runs *after* gap-fill so a missing-role-widened sector
    can still be re-tasked to a threat; mode adjustment runs last so an
    `engage` tightening overrides any earlier widening.
    """
    plans = v_formation_sectors(role_to_robot_id, alive_robot_ids, roles=roles)
    plans = apply_gap_fill(plans, role_to_robot_id, roles=roles)
    plans = apply_threat_focus(
        plans, threat_bearings_deg, focus_half_width_deg=focus_half_width_deg)
    plans = apply_mode_adjustment(plans, mode, role_to_robot_id=role_to_robot_id)
    return plans


class SectorAssignDispatcher:
    """Stateful publisher helper.

    Holds the last published plan per robot + the last periodic-publish
    timestamp. Callers drive it via `due_messages` (periodic + diff) and
    `event_messages` (immediate, e.g. on sector change). Output is a list
    of SectorAssign objects ready to publish.
    """

    def __init__(self, valid_period_sec: int = 10):
        self.valid_period_sec = int(valid_period_sec)
        self._sequence: int = 0
        # None until first publish so now_ms=0 is treated as a real timestamp,
        # not as the "never published" sentinel.
        self._last_periodic_ms: Optional[int] = None
        self._last_sent: Dict[int, SectorPlan] = {}

    def due_messages(
        self,
        now_ms: int,
        plans: Sequence[SectorPlan],
    ) -> List[SectorAssign]:
        """Return messages to publish this tick.

        Publishes the full plan set every PERIOD_MS, AND on any change
        from the previously sent plan (per-robot diff). The first call
        always publishes the full set.
        """
        if not plans:
            return []
        plan_by_id = {p.robot_id: p for p in plans}

        periodic_due = (
            self._last_periodic_ms is None
            or (now_ms - self._last_periodic_ms) >= PERIOD_MS
        )
        changed_ids = {
            rid for rid, p in plan_by_id.items()
            if self._last_sent.get(rid) != p
        }
        # Robots dropped from the roster don't get a message — followers
        # rely on valid_period_sec to age out their cached assignment.
        target_ids = (
            set(plan_by_id) if periodic_due else changed_ids
        )
        if not target_ids:
            return []

        out = [
            self._build(plan_by_id[rid], now_ms)
            for rid in sorted(target_ids)
        ]
        for rid in target_ids:
            self._last_sent[rid] = plan_by_id[rid]
        if periodic_due:
            self._last_periodic_ms = now_ms
        return out

    def event_messages(
        self,
        now_ms: int,
        plans: Sequence[SectorPlan],
    ) -> List[SectorAssign]:
        """Force-publish the given plans (e.g. after a threat handoff).

        Updates last-sent state but does NOT reset the periodic timer —
        the next 10 s tick will still emit a periodic refresh.
        """
        out = [self._build(p, now_ms) for p in plans]
        for p in plans:
            self._last_sent[p.robot_id] = p
        return out

    def _build(self, p: SectorPlan, now_ms: int) -> SectorAssign:
        self._sequence += 1
        msg = SectorAssign(
            sequence=self._sequence,
            robot_id=p.robot_id,
            sector_start_deg=p.sector_start_deg,
            sector_end_deg=p.sector_end_deg,
            valid_period_sec=self.valid_period_sec,
            priority=p.priority,
            mode_hint=p.mode_hint,
            timestamp_ms=now_ms,
        )
        msg.validate()
        return msg


def filter_for_robot(
    msg: SectorAssign,
    my_robot_id: int,
    now_ms: Optional[int] = None,
) -> Optional[SectorAssign]:
    """Return msg if it targets my_robot_id AND has not expired, else None.

    `now_ms` is only used for the expiry check; pass None to skip it
    (useful when the consumer has its own freshness logic).
    """
    if msg.robot_id != my_robot_id:
        return None
    if now_ms is not None and msg.is_expired(now_ms):
        return None
    return msg


__all__ = (
    "DEPUTY_PHASE_OFFSET_DEG",
    "PERIOD_MS",
    "STANDARD_V_FORMATION_ROLES",
    "V_FORMATION_MODES",
    "V_FORMATION_MODE_DEFAULT",
    "V_FORMATION_MODE_DEFENSIVE",
    "V_FORMATION_MODE_ENGAGE",
    "SectorPlan",
    "SectorAssignDispatcher",
    "apply_gap_fill",
    "apply_mode_adjustment",
    "apply_threat_focus",
    "compute_v_formation_sectors",
    "equal_split",
    "filter_for_robot",
    "sector_center_deg",
    "sector_width_deg",
    "union_sectors",
    "v_formation_sectors",
)
