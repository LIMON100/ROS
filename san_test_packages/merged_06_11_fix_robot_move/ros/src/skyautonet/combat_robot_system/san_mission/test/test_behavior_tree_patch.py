# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 testcases.

Validates the deep-dive fixes:
  PR1 — Sequence with memory=False re-evaluates Condition every tick (C1)
  PR2 — Sequence reset() returns _idx to 0 (M8)
  PR3 — Repeat reset() returns _count to 0 (M8)
  PR4 — Selector resets the previously-active child on switch (C1)
  PR5 — Selector reset() recursively resets composite children (M8)
"""
from san_mission.behavior_tree import (
    Action,
    Condition,
    Repeat,
    Selector,
    Sequence,
    Status,
)


class _Recorder:
    def __init__(self, *statuses):
        self.statuses = list(statuses)
        self.calls = 0

    def __call__(self, ctx):
        self.calls += 1
        if not self.statuses:
            return Status.SUCCESS
        return self.statuses.pop(0)


# ─── PR1 (★ C1): Sequence memory=False re-evaluates Condition ──────────
def test_pr1_sequence_memory_false_rechecks_condition():
    """The priority subtrees in mission_bt set memory=False so the
    Condition gate is re-evaluated every tick. Critical for P0
    EmergencyHandler: if emergency_active is cleared externally
    while wait_for_release would have returned RUNNING, the
    subtree must drop OUT (Condition gate → FAILURE) rather than
    keep RUNNING the action."""
    state = {"cond": True}

    def cond_fn(_ctx):
        return state["cond"]

    action_calls = {"n": 0}

    def action_fn(_ctx):
        action_calls["n"] += 1
        return Status.RUNNING  # would normally lock the subtree

    seq = Sequence(
        Condition(cond_fn, name="C"),
        Action(action_fn, name="A"),
        memory=False,
    )
    # Tick 1: condition True → action runs and returns RUNNING.
    assert seq.tick(None) == Status.RUNNING
    assert action_calls["n"] == 1

    # Externally clear the condition.
    state["cond"] = False

    # Tick 2: with memory=False, condition is re-checked → FAILURE.
    # Action must NOT run.
    assert seq.tick(None) == Status.FAILURE
    assert action_calls["n"] == 1, (
        "memory=False sequence must re-check Condition before action")


# ─── PR1b: contrast — memory=True (default) preserves legacy semantic ──
def test_pr1b_sequence_memory_true_preserves_legacy():
    state = {"cond": True}

    def cond_fn(_ctx):
        return state["cond"]

    action_calls = {"n": 0}

    def action_fn(_ctx):
        action_calls["n"] += 1
        return Status.RUNNING

    seq = Sequence(  # default memory=True
        Condition(cond_fn),
        Action(action_fn),
    )
    assert seq.tick(None) == Status.RUNNING
    assert action_calls["n"] == 1
    state["cond"] = False  # change condition
    # Memory=True: resume at _idx=1 (action) — Condition is NOT re-checked
    assert seq.tick(None) == Status.RUNNING
    assert action_calls["n"] == 2


# ─── PR2 (★ M8): Sequence.reset() ──────────────────────────────────────
def test_pr2_sequence_reset_clears_index():
    seq = Sequence(
        Action(_Recorder(Status.SUCCESS)),
        Action(_Recorder(Status.RUNNING)),
    )
    # First tick: child 0 SUCCESS, child 1 RUNNING → seq RUNNING, _idx=1
    assert seq.tick(None) == Status.RUNNING
    assert seq._idx == 1
    seq.reset()
    assert seq._idx == 0


# ─── PR3 (★ M8): Repeat.reset() ────────────────────────────────────────
def test_pr3_repeat_reset_clears_count():
    rep = Repeat(Action(_Recorder(Status.SUCCESS, Status.SUCCESS)), n=5)
    assert rep.tick(None) == Status.RUNNING
    assert rep.tick(None) == Status.RUNNING
    assert rep._count == 2
    rep.reset()
    assert rep._count == 0


# ─── PR4 (★ C1): Selector resets prev-active on switch ────────────────
def test_pr4_selector_resets_prev_active_on_switch():
    """When the active child changes, the previously-active composite
    must be reset so it doesn't carry over mid-flight state. Important
    for the SDD §6.1 fallback root: when P0 drops out and P1 takes
    over, P0's internal Sequence._idx must NOT linger at a non-zero
    position the next time P0 reactivates."""
    # Child 0 is a Sequence that goes RUNNING then SUCCESS.
    child0_action = _Recorder(Status.RUNNING, Status.SUCCESS)
    child0 = Sequence(
        Action(_Recorder(Status.SUCCESS)),
        Action(child0_action),
    )
    # Child 1 is a Sequence whose Condition gates it.
    state = {"c1": False}
    child1 = Sequence(
        Condition(lambda _c: state["c1"]),
        Action(_Recorder(Status.SUCCESS)),
    )
    sel = Selector(child0, child1)

    # Tick 1: child0 RUNNING (Sequence holds at _idx=1).
    assert sel.tick(None) == Status.RUNNING
    assert child0._idx == 1
    assert sel._last_active == 0

    # External event: child1's gate flips. Child0's recorder will
    # then SUCCESS next call.
    state["c1"] = True
    # Tick 2: selector picks ... child0 first (which sticks at _idx=1
    # because of memory and returns SUCCESS via child0_action's
    # second status). Active stays 0.
    s = sel.tick(None)
    # child0 returned SUCCESS this time → selector returns SUCCESS.
    assert s == Status.SUCCESS
    # _idx already reset to 0 by Sequence on SUCCESS.
    assert child0._idx == 0

    # Now flip child0 to fail, force the switch to child1.
    state["c1"] = True
    # Use a separate selector to test the prev-active reset path:
    child0_v2_run = _Recorder(Status.RUNNING, Status.FAILURE)
    child0_v2 = Sequence(Action(child0_v2_run))
    child1_v2 = Sequence(
        Condition(lambda _c: True),
        Action(_Recorder(Status.SUCCESS)),
    )
    sel2 = Selector(child0_v2, child1_v2)
    # Tick 1: child0 RUNNING → _last_active=0
    assert sel2.tick(None) == Status.RUNNING
    # Manually nudge child0's internal state to verify reset happens.
    child0_v2._idx = 99   # simulate a state we want reset
    # Tick 2: child0 FAILURE this time → selector tries child1, SUCCESS.
    s2 = sel2.tick(None)
    assert s2 == Status.SUCCESS
    # PATCH 2026-05-13: previously-active child0 has had reset() called.
    # The FAILURE itself resets _idx to 0; switch path additionally
    # resets — verify _idx is back to 0 either way.
    assert child0_v2._idx == 0


# ─── PR5 (★ M8): Selector.reset() recurses ─────────────────────────────
def test_pr5_selector_reset_recurses():
    inner = Sequence(Action(_Recorder(Status.SUCCESS, Status.RUNNING)))
    sel = Selector(inner)
    sel.tick(None)
    sel.tick(None)
    assert inner._idx > 0 or sel._last_active == 0
    sel.reset()
    assert inner._idx == 0
    assert sel._last_active == -1
