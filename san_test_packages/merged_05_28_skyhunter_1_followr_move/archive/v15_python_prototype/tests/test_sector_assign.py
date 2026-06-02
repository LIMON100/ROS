"""Tests for surveillance sector assignment (/swarm/surveillance/sector_assign)."""
import pytest

from core.messages import (
    SECTOR_MODE_FIXED,
    SECTOR_MODE_SWEEP,
    SECTOR_MODE_TRACK,
    SECTOR_PRIORITY_OVERLAP,
    SECTOR_PRIORITY_PRIMARY,
    SECTOR_PRIORITY_THREAT_FOCUS,
    SectorAssign,
)
from swarm.sector_assign import (
    PERIOD_MS,
    STANDARD_V_FORMATION_ROLES,
    V_FORMATION_MODE_DEFAULT,
    V_FORMATION_MODE_DEFENSIVE,
    V_FORMATION_MODE_ENGAGE,
    SectorAssignDispatcher,
    SectorPlan,
    apply_gap_fill,
    apply_mode_adjustment,
    apply_threat_focus,
    compute_v_formation_sectors,
    equal_split,
    filter_for_robot,
    sector_center_deg,
    sector_width_deg,
    union_sectors,
    v_formation_sectors,
)

# ─── SectorAssign dataclass ────────────────────────────────────────────

def test_validate_accepts_in_range_values():
    msg = SectorAssign(
        sequence=1, robot_id=2,
        sector_start_deg=-45.0, sector_end_deg=45.0,
        valid_period_sec=10,
        priority=SECTOR_PRIORITY_PRIMARY,
        mode_hint=SECTOR_MODE_SWEEP,
        timestamp_ms=1_000,
    )
    msg.validate()  # should not raise


@pytest.mark.parametrize("start,end", [(-181.0, 0.0), (0.0, 181.0), (200.0, 300.0)])
def test_validate_rejects_out_of_range_angles(start, end):
    msg = SectorAssign(sector_start_deg=start, sector_end_deg=end)
    with pytest.raises(ValueError):
        msg.validate()


def test_validate_rejects_unknown_priority():
    msg = SectorAssign(priority="bogus")
    with pytest.raises(ValueError):
        msg.validate()


def test_validate_rejects_unknown_mode_hint():
    msg = SectorAssign(mode_hint="bogus")
    with pytest.raises(ValueError):
        msg.validate()


def test_validate_rejects_negative_valid_period():
    msg = SectorAssign(valid_period_sec=-1)
    with pytest.raises(ValueError):
        msg.validate()


def test_is_expired_respects_valid_period():
    msg = SectorAssign(valid_period_sec=10, timestamp_ms=1_000)
    assert msg.is_expired(now_ms=5_000) is False
    assert msg.is_expired(now_ms=11_000) is False   # exactly 10 s
    assert msg.is_expired(now_ms=11_001) is True


def test_is_expired_zero_period_never_expires():
    msg = SectorAssign(valid_period_sec=0, timestamp_ms=1_000)
    assert msg.is_expired(now_ms=10_000_000) is False


def test_all_priority_and_mode_constants_validate():
    for prio in (SECTOR_PRIORITY_PRIMARY,
                 SECTOR_PRIORITY_THREAT_FOCUS,
                 SECTOR_PRIORITY_OVERLAP):
        for mode in (SECTOR_MODE_SWEEP, SECTOR_MODE_TRACK, SECTOR_MODE_FIXED):
            SectorAssign(priority=prio, mode_hint=mode).validate()


# ─── equal_split policy ─────────────────────────────────────────────────

def test_equal_split_two_robots_180_arc():
    plans = equal_split([10, 20], coverage_deg=180.0, center_deg=0.0)
    assert len(plans) == 2
    assert plans[0].robot_id == 10
    assert plans[0].sector_start_deg == pytest.approx(-90.0)
    assert plans[0].sector_end_deg   == pytest.approx(0.0)
    assert plans[1].sector_start_deg == pytest.approx(0.0)
    assert plans[1].sector_end_deg   == pytest.approx(90.0)


def test_equal_split_empty_roster_returns_empty():
    assert equal_split([]) == []


def test_equal_split_360_wraps_into_range():
    plans = equal_split([1, 2, 3, 4], coverage_deg=360.0)
    for p in plans:
        assert -180.0 <= p.sector_start_deg <= 180.0
        assert -180.0 <= p.sector_end_deg   <= 180.0


# ─── SectorAssignDispatcher ─────────────────────────────────────────────

def _plans():
    return [
        SectorPlan(robot_id=1, sector_start_deg=-90.0, sector_end_deg=0.0),
        SectorPlan(robot_id=2, sector_start_deg=0.0,   sector_end_deg=90.0),
    ]


def test_dispatcher_first_call_emits_full_set():
    d = SectorAssignDispatcher()
    out = d.due_messages(now_ms=0, plans=_plans())
    assert {m.robot_id for m in out} == {1, 2}
    # All carry the same timestamp + monotonically increasing sequence.
    assert all(m.timestamp_ms == 0 for m in out)
    assert [m.sequence for m in out] == sorted(m.sequence for m in out)


def test_dispatcher_skips_when_nothing_changed_within_period():
    d = SectorAssignDispatcher()
    d.due_messages(now_ms=0, plans=_plans())
    # 1 s later, same plans, no event — must be silent
    out = d.due_messages(now_ms=1_000, plans=_plans())
    assert out == []


def test_dispatcher_periodic_refresh_at_10s():
    d = SectorAssignDispatcher()
    d.due_messages(now_ms=0, plans=_plans())
    out = d.due_messages(now_ms=PERIOD_MS, plans=_plans())
    assert {m.robot_id for m in out} == {1, 2}


def test_dispatcher_event_emits_only_changed_robot():
    d = SectorAssignDispatcher()
    d.due_messages(now_ms=0, plans=_plans())
    changed = [
        SectorPlan(robot_id=1, sector_start_deg=-90.0, sector_end_deg=0.0),
        SectorPlan(robot_id=2, sector_start_deg=10.0,  sector_end_deg=100.0),
    ]
    # Mid-period, only robot 2's sector moved → only robot 2 should be sent.
    out = d.due_messages(now_ms=1_000, plans=changed)
    assert [m.robot_id for m in out] == [2]


def test_dispatcher_event_messages_force_publish():
    d = SectorAssignDispatcher()
    d.due_messages(now_ms=0, plans=_plans())
    out = d.event_messages(now_ms=500, plans=_plans())
    # event_messages publishes unconditionally
    assert {m.robot_id for m in out} == {1, 2}


def test_dispatcher_assigns_configured_valid_period():
    d = SectorAssignDispatcher(valid_period_sec=7)
    out = d.due_messages(now_ms=0, plans=_plans())
    assert all(m.valid_period_sec == 7 for m in out)


# ─── filter_for_robot ───────────────────────────────────────────────────

def test_filter_drops_other_robots():
    msg = SectorAssign(robot_id=5, timestamp_ms=0, valid_period_sec=10)
    assert filter_for_robot(msg, my_robot_id=3) is None
    assert filter_for_robot(msg, my_robot_id=5) is msg


def test_filter_drops_expired_when_now_provided():
    msg = SectorAssign(robot_id=5, timestamp_ms=0, valid_period_sec=1)
    assert filter_for_robot(msg, my_robot_id=5, now_ms=500) is msg
    assert filter_for_robot(msg, my_robot_id=5, now_ms=2_000) is None


def test_filter_skips_expiry_when_now_none():
    msg = SectorAssign(robot_id=5, timestamp_ms=0, valid_period_sec=1)
    # 100 s after timestamp, still passes — caller opts out of expiry check.
    assert filter_for_robot(msg, my_robot_id=5, now_ms=None) is msg


# ─── sector geometry helpers ────────────────────────────────────────────

@pytest.mark.parametrize("start,end,expected", [
    (-30.0, +30.0,   60.0),                  # straightforward
    (+150.0, -150.0, 60.0),                  # wraps through ±180
    (-150.0, +150.0, 300.0),                 # huge non-wrapping arc
    (0.0, 0.0,       0.0),                   # degenerate
])
def test_sector_width_handles_wraparound(start, end, expected):
    assert sector_width_deg(start, end) == pytest.approx(expected)


@pytest.mark.parametrize("start,end,expected_center", [
    (-30.0, +30.0,   0.0),
    (+150.0, -150.0, 180.0),                 # rear center, wrap
    (+90.0,  +150.0, 120.0),
    (-90.0,  -30.0,  -60.0),
])
def test_sector_center_handles_wraparound(start, end, expected_center):
    c = sector_center_deg(start, end)
    # 180 ≡ -180 — normalize for comparison
    assert pytest.approx((c + 360) % 360) == (expected_center + 360) % 360


def test_union_sectors_adjacent_no_wrap():
    # F1 (-90, -30) ∪ F3 (-120, -60): overlapping non-wrap; union is the
    # left-most start to the right-most end.
    assert union_sectors(-90.0, -30.0, -120.0, -60.0) == (-120.0, -30.0)


def test_union_sectors_shares_endpoint():
    # Leader (-30, +30) shares endpoint with F2 (+30, +90).
    assert union_sectors(-30.0, +30.0, +30.0, +90.0) == (-30.0, +90.0)


def test_union_sectors_wrap_through_pi():
    # F6 ∪ Hub crosses ±180°; the union must be the 120° rear-right arc.
    assert union_sectors(+90.0, +150.0, +150.0, -150.0) == (+90.0, -150.0)


def test_union_sectors_one_contains_the_other():
    # A fully contains B → union = A.
    result = union_sectors(-120.0, +120.0, -30.0, +30.0)
    assert result == (-120.0, +120.0)


# ─── V-formation base assignment ────────────────────────────────────────

def _full_role_map():
    """Standard mapping used by most V-formation tests.

    v1.5 (DCN-2026-001 D-001): full 9-slot mapping including Deputy.
    Leader=1, Hub=2, Deputy=3, F1-F6=4..9. Note that real v1.5 squadrons
    cap deployment at 8 robots — these unit tests intentionally provide
    all 9 slots to exercise the table without gap_fill widening.
    """
    return {
        "leader":    1,
        "hub":       2,
        "deputy":    3,
        "follower1": 4,
        "follower2": 5,
        "follower3": 6,
        "follower4": 7,
        "follower5": 8,
        "follower6": 9,
    }


def test_v_formation_full_swarm_produces_9_plans():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    # v1.5 (DCN-2026-001 D-001): 9 logical slots = Leader + F1..F6 +
    # Hub + Deputy. Real squadrons deploy at most 8, but the full
    # 9-slot table is exercised here.
    assert len(plans) == 9
    # Each role's sector matches the standard table.
    by_id = {p.robot_id: p for p in plans}
    for role, (s, e, _prio) in STANDARD_V_FORMATION_ROLES.items():
        rid = rm[role]
        assert by_id[rid].sector_start_deg == pytest.approx(s)
        assert by_id[rid].sector_end_deg   == pytest.approx(e)


def test_v_formation_skips_unmapped_role():
    # If a role has no robot mapping, no plan is produced for it.
    # v1.5 (DCN-2026-001 D-001): hub maps to robot_id 2 (was 8 in v1.3).
    rm = _full_role_map()
    rm.pop("follower7", None)        # unmapped anyway in the standard table
    rm.pop("hub")                    # robot_id 2
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    assert all(p.robot_id != 2 for p in plans)
    # Total slots in table = 9 (v1.5: leader + F1..F6 + hub + deputy);
    # remove hub → 8 plans remain.
    assert len(plans) == 8


def test_v_formation_skips_dead_robot():
    rm = _full_role_map()
    # F2 (robot 3) drops out — no plan for it.
    alive = [rid for rid in rm.values() if rid != 3]
    plans = v_formation_sectors(rm, alive_robot_ids=alive)
    assert all(p.robot_id != 3 for p in plans)


def test_v_formation_hub_sector_is_wrap_aware():
    # v1.5 (DCN-2026-001 D-001): Hub UGV is robot_id 2 (was 8 in v1.3).
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    hub = next(p for p in plans if p.robot_id == 2)
    # Hub: (+150, -150) — CCW through ±180, total 60° rear coverage.
    assert hub.sector_start_deg == pytest.approx(+150.0)
    assert hub.sector_end_deg   == pytest.approx(-150.0)
    assert sector_width_deg(hub.sector_start_deg, hub.sector_end_deg) \
        == pytest.approx(60.0)


# ─── gap-fill ──────────────────────────────────────────────────────────

def test_gap_fill_no_missing_returns_input_unchanged():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_gap_fill(plans, role_to_robot_id=rm)
    # Same robot set, same sectors.
    assert {p.robot_id for p in out} == {p.robot_id for p in plans}
    by_id = {p.robot_id: p for p in plans}
    for p in out:
        assert p.sector_start_deg == by_id[p.robot_id].sector_start_deg
        assert p.sector_end_deg   == by_id[p.robot_id].sector_end_deg


def test_gap_fill_widens_neighbour_when_follower_lost():
    # F1 (-90,-30) is lost; the closest alive role by sector center is
    # F3 (-120,-60). Expect F3's plan to widen to (-120, -30).
    rm = _full_role_map()
    alive = [rid for rid in rm.values() if rid != rm["follower1"]]
    plans = v_formation_sectors(rm, alive_robot_ids=alive)
    out = apply_gap_fill(plans, role_to_robot_id=rm)
    f3 = next(p for p in out if p.robot_id == rm["follower3"])
    assert f3.sector_start_deg == pytest.approx(-120.0)
    assert f3.sector_end_deg   == pytest.approx(-30.0)
    assert f3.priority == SECTOR_PRIORITY_OVERLAP


def test_gap_fill_handles_hub_lost():
    # Hub lost → nearest by center is F5 or F6 (both 60° away from rear).
    # Deterministic tiebreak: alphabetically F5 < F6, so F5 wins.
    rm = _full_role_map()
    alive = [rid for rid in rm.values() if rid != rm["hub"]]
    plans = v_formation_sectors(rm, alive_robot_ids=alive)
    out = apply_gap_fill(plans, role_to_robot_id=rm)
    f5 = next(p for p in out if p.robot_id == rm["follower5"])
    # F5 (-150,-90) ∪ Hub (+150,-150) → (+150, -90), wraps through ±180.
    assert f5.sector_start_deg == pytest.approx(+150.0)
    assert f5.sector_end_deg   == pytest.approx(-90.0)
    assert f5.priority == SECTOR_PRIORITY_OVERLAP


# ─── threat focus ──────────────────────────────────────────────────────

def test_threat_focus_no_threats_is_identity():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_threat_focus(plans, threat_bearings_deg=[])
    # Same content, just deep-copied — verify shape.
    assert len(out) == len(plans)
    for a, b in zip(sorted(plans, key=lambda p: p.robot_id),
                    sorted(out,   key=lambda p: p.robot_id),
                    strict=True):
        assert a.sector_start_deg == b.sector_start_deg
        assert a.sector_end_deg   == b.sector_end_deg
        assert a.priority         == b.priority


def test_threat_focus_retargets_nearest_sector():
    # Threat at -60° → F1 (center -60°) is closest; gets a 30° window
    # centered on the threat with priority=threat_focus and mode=track.
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_threat_focus(plans, threat_bearings_deg=[-60.0],
                             focus_half_width_deg=15.0)
    f1 = next(p for p in out if p.robot_id == rm["follower1"])
    assert f1.priority == SECTOR_PRIORITY_THREAT_FOCUS
    assert f1.mode_hint == SECTOR_MODE_TRACK
    assert f1.sector_start_deg == pytest.approx(-75.0)
    assert f1.sector_end_deg   == pytest.approx(-45.0)


def test_threat_focus_backup_gets_overlap():
    # With a threat at -60°, the second-closest alive sector should be
    # bumped to OVERLAP + TRACK to act as a tracking backup.
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_threat_focus(plans, threat_bearings_deg=[-60.0])
    # F3 (center -90) and leader (center 0) are next-nearest; pick the one
    # whose center is at distance 30 from -60 (F3).
    f3 = next(p for p in out if p.robot_id == rm["follower3"])
    assert f3.priority == SECTOR_PRIORITY_OVERLAP
    assert f3.mode_hint == SECTOR_MODE_TRACK


# ─── mode adjustment ───────────────────────────────────────────────────

def test_mode_adjustment_default_is_identity():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_mode_adjustment(
        plans, V_FORMATION_MODE_DEFAULT, role_to_robot_id=rm)
    for a, b in zip(sorted(plans, key=lambda p: p.robot_id),
                    sorted(out,   key=lambda p: p.robot_id),
                    strict=True):
        assert a.sector_start_deg == b.sector_start_deg
        assert a.sector_end_deg   == b.sector_end_deg


def test_mode_adjustment_defensive_is_identity():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_mode_adjustment(
        plans, V_FORMATION_MODE_DEFENSIVE, role_to_robot_id=rm)
    by_in  = {p.robot_id: p for p in plans}
    by_out = {p.robot_id: p for p in out}
    for rid in by_in:
        assert by_in[rid].sector_start_deg == by_out[rid].sector_start_deg
        assert by_in[rid].sector_end_deg   == by_out[rid].sector_end_deg


def test_mode_adjustment_engage_tightens_front_only():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    out = apply_mode_adjustment(
        plans, V_FORMATION_MODE_ENGAGE, role_to_robot_id=rm)
    by_out = {p.robot_id: p for p in out}
    # Leader sector tightens from 60° → 30°, still centered on 0°.
    leader = by_out[rm["leader"]]
    assert sector_width_deg(leader.sector_start_deg, leader.sector_end_deg) \
        == pytest.approx(30.0)
    assert sector_center_deg(leader.sector_start_deg, leader.sector_end_deg) \
        == pytest.approx(0.0)
    # F4 (overlap role, NOT forward primary) is unchanged.
    f4 = by_out[rm["follower4"]]
    assert sector_width_deg(f4.sector_start_deg, f4.sector_end_deg) \
        == pytest.approx(60.0)


def test_mode_adjustment_rejects_unknown_mode():
    rm = _full_role_map()
    plans = v_formation_sectors(rm, alive_robot_ids=rm.values())
    with pytest.raises(ValueError):
        apply_mode_adjustment(plans, "bogus", role_to_robot_id=rm)


# ─── orchestrator ──────────────────────────────────────────────────────

def test_compute_v_formation_full_pipeline_clean_swarm():
    rm = _full_role_map()
    plans = compute_v_formation_sectors(
        role_to_robot_id=rm,
        alive_robot_ids=rm.values(),
    )
    # No threats, no missing roles, default mode → identical to base table.
    by_id = {p.robot_id: p for p in plans}
    for role, (s, e, _prio) in STANDARD_V_FORMATION_ROLES.items():
        rid = rm[role]
        assert by_id[rid].sector_start_deg == pytest.approx(s)
        assert by_id[rid].sector_end_deg   == pytest.approx(e)


def test_compute_v_formation_engage_mode_with_missing_follower():
    rm = _full_role_map()
    alive = [rid for rid in rm.values() if rid != rm["follower6"]]
    plans = compute_v_formation_sectors(
        role_to_robot_id=rm,
        alive_robot_ids=alive,
        mode=V_FORMATION_MODE_ENGAGE,
    )
    # Engage tightens leader/F1/F2; F6 missing widens its neighbour.
    by_id = {p.robot_id: p for p in plans}
    leader = by_id[rm["leader"]]
    assert sector_width_deg(leader.sector_start_deg, leader.sector_end_deg) \
        == pytest.approx(30.0)
    # F6 is gone; its plan must not be in the output.
    assert rm["follower6"] not in by_id


def test_compute_v_formation_with_threat_overrides_sector():
    rm = _full_role_map()
    plans = compute_v_formation_sectors(
        role_to_robot_id=rm,
        alive_robot_ids=rm.values(),
        threat_bearings_deg=[-60.0],
    )
    f1 = next(p for p in plans if p.robot_id == rm["follower1"])
    assert f1.priority == SECTOR_PRIORITY_THREAT_FOCUS
    assert f1.mode_hint == SECTOR_MODE_TRACK
