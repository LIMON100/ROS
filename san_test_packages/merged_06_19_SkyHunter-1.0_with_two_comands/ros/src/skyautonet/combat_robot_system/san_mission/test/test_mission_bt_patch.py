# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 mission_bt deep-dive tests.

Validates:
  PM1 (★ C1)  EmergencyHandler drops out the moment emergency_active
              is cleared externally, even if it was RUNNING last tick.
  PM2 (★ C6)  _wait_for_release no longer mutates emergency_active /
              emergency_release_armed — the subscription owns those
              fields. The BT just observes.
  PM3 (★ C1)  Cross-priority isolation: P0 drops → P1 picks up cleanly
              with no leftover _idx in P0's Sequence.
"""
from san_mission.behavior_tree import Status
from san_mission.mission_bt import (
    ExtendedMissionContext,
    build_mission_tree,
    _wait_for_release,
)


def _make_ctx(**priority_kwargs):
    """Make an ExtendedMissionContext with sane defaults."""
    ctx = ExtendedMissionContext()
    ctx.pose_xy = (0.0, 0.0)
    ctx.yaw_rad = 0.0
    ctx.battery_percent = 80.0
    for k, v in priority_kwargs.items():
        setattr(ctx.priority, k, v)
    return ctx


# ─── PM1 (★ C1): Emergency drops out when cleared mid-flight ──────────
def test_pm1_emergency_drops_when_cleared_externally():
    """Without memory=False the EmergencyHandler Sequence would hold
    its _idx at WaitForRelease (returning RUNNING) and never re-check
    the Condition. With the PATCH, the Condition gate is re-evaluated
    every tick — when emergency_active is cleared externally, P0
    drops out at the next tick and the Selector advances to lower
    priorities."""
    tree = build_mission_tree()
    ctx  = _make_ctx(emergency_active=True)

    # Tick 1: P0 fires, WaitForRelease → RUNNING.
    assert tree.tick(ctx) == Status.RUNNING

    # External clear (mimics _on_manual_override OVERRIDE_RELEASE).
    ctx.priority.emergency_active = False
    ctx.priority.emergency_release_armed = True

    # Tick 2: Condition (emergency_active) is now False → P0 FAILS
    # → Selector advances → normal flow runs → SUCCESS.
    s = tree.tick(ctx)
    assert s == Status.SUCCESS, (
        "After external clear, P0 must NOT hold the BT in RUNNING")


# ─── PM2 (★ C6): _wait_for_release is a pure observer ─────────────────
def test_pm2_wait_for_release_does_not_mutate_state():
    """The PATCH moved emergency state ownership entirely to the
    subscription side. _wait_for_release just reads — it must not
    write emergency_active or emergency_release_armed."""
    ctx = _make_ctx(
        emergency_active=True,
        emergency_release_armed=True,
    )
    # Pre-state.
    assert ctx.priority.emergency_active is True
    assert ctx.priority.emergency_release_armed is True

    # Call _wait_for_release. With the PATCH: returns RUNNING because
    # emergency_active is still True (handshake not complete).
    s = _wait_for_release(ctx)
    assert s == Status.RUNNING
    # And the BT did NOT mutate either field.
    assert ctx.priority.emergency_active is True
    assert ctx.priority.emergency_release_armed is True

    # Now simulate the subscription completing the handshake.
    ctx.priority.emergency_active = False
    s = _wait_for_release(ctx)
    assert s == Status.SUCCESS
    # BT still doesn't mutate.
    assert ctx.priority.emergency_release_armed is True


# ─── PM3 (★ C1): cross-priority Sequence state isolation ──────────────
def test_pm3_p0_to_p1_clean_transition():
    """When P0 (EmergencyHandler) drops out and P1 (ManualOverride)
    takes over, P0's internal Sequence._idx must be at 0 (reset),
    not leftover at the WaitForRelease position. Verifies the
    Selector's prev-active reset path through to the priority
    subtree.
    """
    tree = build_mission_tree()
    # Reach into the tree to grab P0's Sequence for inspection.
    p0_seq = tree.children[0]   # EmergencyHandler
    assert p0_seq.name == "EmergencyHandler"

    ctx = _make_ctx(emergency_active=True)
    # Tick 1: P0 fires, RUNNING.
    assert tree.tick(ctx) == Status.RUNNING

    # External clear + arm release atomically (as subscription does).
    ctx.priority.emergency_active = False
    ctx.priority.emergency_release_armed = True
    # Also enter manual mode so P1 picks up.
    ctx.priority.manual_mode_active = True
    ctx.priority.manual_cmd_vel = (0.5, 0.0)

    # Tick 2: P0 Condition False → P0 FAILURE.
    # Selector advances → P1 Condition True → P1 SUCCESS.
    s = tree.tick(ctx)
    assert s == Status.SUCCESS

    # ★ PATCH 2026-05-13: P0's internal _idx is 0 (not stuck at 2).
    assert p0_seq._idx == 0, (
        "P0 must reset its internal Sequence index after dropping")
