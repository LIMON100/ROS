"""S15-1 — 8-robot V-formation 360° surveillance coverage scenario.

Spec: SAN-TST-INT-001 v1.5 §7 / S15-1.

Scenario-level integration test: builds the canonical 8-slot V-formation
(Leader + Hub + Deputy + F1-F5) and verifies the dispatcher's
compute_v_formation_sectors output behaves end-to-end:

  * 360° coverage of the union of all assigned sectors
  * Threat focus reassignment retargets exactly 2 followers (primary +
    backup) within one dispatch call (the spec's "≤1s response" is a
    timing assertion against a live ROS2 graph; in pure-compute this
    is one synchronous call, so the assertion is "no extra dispatch
    cycle needed")
  * Gap-fill on a follower drop-out maintains 360° coverage
  * Per-follower sweep speed from the pan-tilt controller stays inside
    the gimbal's hardware envelope and matches the (sector_width - HFOV)
    × 2 / period formula
  * Deputy UGV (S3) shadows the Hub's rear-180° sector with a 180°
    phase offset (SDD-SUR v1.5 §3.2)

Out-of-scope vs the ROS2 spec: 10 s periodic-dispatch timing (the
dispatcher's 10-s cadence is covered in test_sector_assign.py at the
unit level; the scenario doesn't need to re-time it here).

v1.5 (DCN-2026-001 D-001): the squadron is capped at 8 robots; the
v1.3-era F7 dynamic spare role is removed. Deputy UGV (S3) is now a
first-class slot (priority SHADOW) sweeping the Hub's rear sector with
a 180° phase offset to double rear-band revisit rate.
"""
from __future__ import annotations

import pytest

from control.pantilt_controller import (
    DEFAULT_HFOV_DEG,
    PanTiltController,
    sweep_speed_dps,
)
from core.messages import (
    PAN_TILT_MAX_SPEED_DPS,
    PAN_TILT_MODE_FIXED,
    SECTOR_MODE_TRACK,
    SECTOR_PRIORITY_OVERLAP,
    SECTOR_PRIORITY_SHADOW,
    SECTOR_PRIORITY_THREAT_FOCUS,
)
from swarm.sector_assign import (
    DEPUTY_PHASE_OFFSET_DEG,
    STANDARD_V_FORMATION_ROLES,
    V_FORMATION_MODE_DEFAULT,
    compute_v_formation_sectors,
)
from tests._s15_helpers import compute_total_coverage_deg, sectors_from_plans

# v1.5 (DCN-2026-001 D-001) canonical 8-slot role map.
# Leader=1, Hub=2, Deputy=3, F1-F5=4..8 — matches swarm_coordinator.hpp
# LEADER_ROBOT_ID/HUB_ROBOT_ID/DEPUTY_ROBOT_ID constants.
_FULL_ROLE_MAP = {
    "leader":    1,
    "hub":       2,
    "deputy":    3,
    "follower1": 4,
    "follower2": 5,
    "follower3": 6,
    "follower4": 7,
    "follower5": 8,
}


def _alive_all() -> list:
    return list(_FULL_ROLE_MAP.values())


# ─── 360° coverage ─────────────────────────────────────────────────────

def test_full_v_formation_covers_full_circle():
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
    )
    coverage = compute_total_coverage_deg(sectors_from_plans(plans))
    assert coverage == pytest.approx(360.0, abs=0.5), (
        f"S15-1: 8-robot V-formation must cover 360°; got {coverage:.1f}°")


def test_engage_mode_trades_coverage_for_forward_resolution():
    """Engage mode tightens Leader/F1/F2 from 60° → 30° each, so the
    union loses 3×30° = 90° around the forward band that the overlap
    roles (F3/F4) don't fully cover. This is *by design* — engage =
    concentrate sensors on the contact bearing. The trade-off should
    be bounded though: never below ~270° (still better than just the
    forward 180°).
    """
    default_plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
        mode=V_FORMATION_MODE_DEFAULT,
    )
    engage_plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
        mode="engage",
    )
    default_cov = compute_total_coverage_deg(sectors_from_plans(default_plans))
    engage_cov  = compute_total_coverage_deg(sectors_from_plans(engage_plans))
    # Trade-off exists (engage < default).
    assert engage_cov < default_cov, (
        "engage mode should narrow coverage vs default; got "
        f"engage={engage_cov:.1f}° >= default={default_cov:.1f}°")
    # Trade-off is bounded — still substantially better than forward
    # 180° alone.
    assert engage_cov >= 270.0, (
        f"engage mode coverage {engage_cov:.1f}° is below the 270° "
        "floor — the overlap + rear roles should still hold")


# ─── threat focus reassignment ─────────────────────────────────────────

def test_threat_at_minus_60_retargets_two_followers_within_one_dispatch():
    """A threat at -60° → F1 (center -60°) becomes the primary tracker;
    F3 (center -90°) becomes the OVERLAP backup tracker. Spec says
    "2-3 followers focused"; we land at exactly 2 (the deterministic
    smallest case)."""
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
        threat_bearings_deg=[-60.0],
    )
    threat_focus_plans = [
        p for p in plans
        if p.priority == SECTOR_PRIORITY_THREAT_FOCUS]
    overlap_track_plans = [
        p for p in plans
        if p.priority == SECTOR_PRIORITY_OVERLAP
        and p.mode_hint == SECTOR_MODE_TRACK]
    assert len(threat_focus_plans) == 1
    assert len(overlap_track_plans) == 1
    focused_ids = {threat_focus_plans[0].robot_id,
                   overlap_track_plans[0].robot_id}
    # Should be F1 + F3 (the two left-of-centre followers nearest -60°).
    assert focused_ids == {_FULL_ROLE_MAP["follower1"],
                           _FULL_ROLE_MAP["follower3"]}


def test_two_threats_dispatch_two_independent_focus_groups():
    """Threats at -60° and +60° → each gets its own primary + backup
    tracker; no follower is double-assigned."""
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
        threat_bearings_deg=[-60.0, +60.0],
    )
    threat_focus = [p for p in plans
                    if p.priority == SECTOR_PRIORITY_THREAT_FOCUS]
    overlap_track = [p for p in plans
                     if p.priority == SECTOR_PRIORITY_OVERLAP
                     and p.mode_hint == SECTOR_MODE_TRACK]
    assert len(threat_focus) == 2
    assert len(overlap_track) == 2
    all_focused = {p.robot_id for p in threat_focus + overlap_track}
    assert len(all_focused) == 4, "no follower should be double-tasked"


# ─── gap-fill on follower drop-out ─────────────────────────────────────

def test_coverage_holds_when_one_follower_lost():
    """Spec line: "follower 1대 탈락 시 360° coverage 유지." Apply a
    single drop-out (F4) and verify the gap-fill widening keeps the
    union at 360°."""
    alive = [rid for rid in _alive_all()
             if rid != _FULL_ROLE_MAP["follower4"]]
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=alive,
    )
    coverage = compute_total_coverage_deg(sectors_from_plans(plans))
    # F4 is an OVERLAP role; its slot is covered by F2 + F6 already, so
    # full 360° survives even without gap-fill widening.
    assert coverage == pytest.approx(360.0, abs=0.5)


def test_coverage_holds_when_two_followers_lost():
    """Harder case: lose F1 AND F3 (both left-side, one primary + one
    overlap). Gap-fill must widen a neighbour to plug the hole. The
    union must still cover 360° (or come close).
    """
    alive = [rid for rid in _alive_all()
             if rid not in (_FULL_ROLE_MAP["follower1"],
                            _FULL_ROLE_MAP["follower3"])]
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=alive,
    )
    coverage = compute_total_coverage_deg(sectors_from_plans(plans))
    # F1 + F3 covered the left front-side together; their union (-120 to
    # -30) is left exposed unless gap-fill widens F5 + Leader. Our
    # gap_fill widens the nearest alive role into the gap, so coverage
    # should still hit 360°. Allow a small epsilon for the +150/-150
    # wrap edges.
    assert coverage >= 355.0, (
        f"S15-1: two-follower-lost coverage = {coverage:.1f}° — "
        "gap-fill failed to compensate")


# ─── sweep-speed compliance via PanTiltController ──────────────────────

def test_every_follower_sweep_speed_within_gimbal_envelope():
    """For every plan the V-formation produces, the pan-tilt controller
    must emit a command with speed ≤ PAN_TILT_MAX_SPEED_DPS (gimbal
    hardware cap, 60 °/s). Sectors that fit inside HFOV come out as
    mode=fixed with speed 0; sectors wider than HFOV sweep.
    """
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
    )
    for plan in plans:
        ctrl = PanTiltController(robot_id=plan.robot_id)
        # Synthesise the SectorAssign the leader would publish.
        from core.messages import SectorAssign
        msg = SectorAssign(
            robot_id=plan.robot_id,
            sector_start_deg=plan.sector_start_deg,
            sector_end_deg=plan.sector_end_deg,
            valid_period_sec=10,
            priority=plan.priority,
            mode_hint=plan.mode_hint,
            timestamp_ms=0,
        )
        cmd = ctrl.on_sector(msg, now_ms=0)
        assert cmd is not None
        assert 0.0 <= cmd.speed_dps <= PAN_TILT_MAX_SPEED_DPS
        # Validate so we catch envelope violations the same way
        # production code would.
        cmd.validate()


def test_default_v_formation_sector_widths_all_fit_hfov():
    """The standard table uses 60° sectors (= HFOV) for the role slots
    actually deployed. Every deployed role's commanded mode should
    therefore be FIXED with speed 0 — the head parks on sector centre
    rather than sweeping. The first operational mode that breaks this
    is `wide` (not asserted here) where F3-F6 take 90° slots.

    v1.5 (DCN-2026-001 D-001): the 8-robot squadron deploys only 5
    followers. The 6th-follower slot in STANDARD_V_FORMATION_ROLES is
    intentionally unfilled, so apply_gap_fill widens exactly one
    neighbour (typically follower4) to absorb its sector. That one
    widened plan will be SWEEP, not FIXED — so we expect
    `len(plans) - 1` fixed-mode commands.
    """
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
        mode=V_FORMATION_MODE_DEFAULT,
    )
    fixed_count = 0
    for plan in plans:
        ctrl = PanTiltController(
            robot_id=plan.robot_id, hfov_deg=DEFAULT_HFOV_DEG)
        from core.messages import SectorAssign
        msg = SectorAssign(
            robot_id=plan.robot_id,
            sector_start_deg=plan.sector_start_deg,
            sector_end_deg=plan.sector_end_deg,
            valid_period_sec=10,
            priority=plan.priority,
            mode_hint=plan.mode_hint,
            phase_offset_deg=plan.phase_offset_deg,
            timestamp_ms=0,
        )
        cmd = ctrl.on_sector(msg, now_ms=0)
        if cmd.mode == PAN_TILT_MODE_FIXED:
            fixed_count += 1
    # v1.5: 5 deployed followers (D-001) + Leader + Hub + Deputy = 8
    # plans. One follower's sector is widened by gap_fill (60° → 90°)
    # to absorb the unfilled follower6 slot — that one is SWEEP. The
    # remaining 7 stay at 60° = HFOV → FIXED.
    assert fixed_count == len(plans) - 1, (
        f"S15-1 v1.5 default mode: expected {len(plans) - 1} FIXED + 1 "
        f"widened-SWEEP plans (gap_fill on unfilled follower6 slot), "
        f"but got {fixed_count} FIXED out of {len(plans)} total")


def test_sweep_speed_formula_matches_spec_examples():
    """Spec table entries from §7.5 of the surveillance dispatcher
    docstring:
        60° sector, 60° HFOV  → 0 °/s   (fits, no sweep)
        90° sector            → 6 °/s   ((90-60)×2/10)
        180° sector           → 24 °/s  ((180-60)×2/10)

    Note the spec's "30°/s" claim corresponds to a 210° sector
    on a 10s period — the formula scales linearly.
    """
    assert sweep_speed_dps(60.0, period_sec=10) == 0.0
    assert sweep_speed_dps(90.0, period_sec=10) == pytest.approx(6.0)
    assert sweep_speed_dps(180.0, period_sec=10) == pytest.approx(24.0)


def test_standard_role_count_is_nine():
    """The v1.5 standard table covers 9 logical roles:
    Leader + F1..F6 (v1.3-era geometry) + Hub + Deputy.

    DCN-2026-001 D-001 caps the squadron at 8 *deployed* robots
    (Leader + Hub + Deputy + 1~5 followers). When only 5 followers are
    deployed, the 6th-follower slot is left unfilled and apply_gap_fill
    widens neighbouring sectors to absorb it. The v1.3-era F7 dynamic
    spare slot is permanently removed."""
    assert len(STANDARD_V_FORMATION_ROLES) == 9
    assert "follower7" not in STANDARD_V_FORMATION_ROLES
    assert "deputy" in STANDARD_V_FORMATION_ROLES


# ─── v1.5 Deputy UGV shadow coverage (SDD-SUR §3.2) ────────────────────

def test_deputy_shadows_hub_sector():
    """v1.5 SDD-SUR §3.2: Deputy UGV (S3) covers exactly the same body
    relative sector as Hub UGV (S2) — same start/end angles. The two
    plans are differentiated by priority (Hub=PRIMARY, Deputy=SHADOW)
    and phase_offset_deg (Hub=0°, Deputy=180°)."""
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
    )
    by_id = {p.robot_id: p for p in plans}
    hub_plan = by_id[_FULL_ROLE_MAP["hub"]]
    deputy_plan = by_id[_FULL_ROLE_MAP["deputy"]]
    assert deputy_plan.sector_start_deg == hub_plan.sector_start_deg
    assert deputy_plan.sector_end_deg == hub_plan.sector_end_deg
    assert deputy_plan.priority == SECTOR_PRIORITY_SHADOW
    assert hub_plan.priority != SECTOR_PRIORITY_SHADOW


def test_deputy_phase_offset_180():
    """v1.5 SDD-SUR §3.2: Deputy's phase_offset_deg is set to 180° so
    that its sweep is in opposition to the Hub's — doubling the rear
    180°-band revisit rate. Non-SHADOW slots have phase_offset_deg == 0."""
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=_alive_all(),
    )
    by_id = {p.robot_id: p for p in plans}
    deputy_plan = by_id[_FULL_ROLE_MAP["deputy"]]
    assert deputy_plan.phase_offset_deg == pytest.approx(
        DEPUTY_PHASE_OFFSET_DEG)
    # Every non-SHADOW plan must have phase_offset_deg == 0.
    for p in plans:
        if p.priority != SECTOR_PRIORITY_SHADOW:
            assert p.phase_offset_deg == 0.0, (
                f"Non-SHADOW plan for robot {p.robot_id} has unexpected "
                f"phase_offset_deg={p.phase_offset_deg}")


def test_minimum_four_robot_squadron_covers_360():
    """v1.5 DCN-2026-001 D-001 minimum squadron = 4 robots:
    Leader + Hub + Deputy + 1 follower. Even with this minimal roster
    the dispatcher must still produce 360° coverage via gap_fill widening
    of the lone follower's sector."""
    # Only Leader (1), Hub (2), Deputy (3), Follower1 (4) alive.
    minimal_alive = [
        _FULL_ROLE_MAP["leader"],
        _FULL_ROLE_MAP["hub"],
        _FULL_ROLE_MAP["deputy"],
        _FULL_ROLE_MAP["follower1"],
    ]
    plans = compute_v_formation_sectors(
        role_to_robot_id=_FULL_ROLE_MAP,
        alive_robot_ids=minimal_alive,
    )
    coverage = compute_total_coverage_deg(sectors_from_plans(plans))
    assert coverage == pytest.approx(360.0, abs=0.5), (
        f"S15-1 v1.5: 4-robot minimum squadron must cover 360° via "
        f"gap_fill widening; got {coverage:.1f}°")
    # Deputy must still be present + SHADOW even in minimum squadron.
    by_id = {p.robot_id: p for p in plans}
    assert _FULL_ROLE_MAP["deputy"] in by_id
    assert by_id[_FULL_ROLE_MAP["deputy"]].priority == SECTOR_PRIORITY_SHADOW
