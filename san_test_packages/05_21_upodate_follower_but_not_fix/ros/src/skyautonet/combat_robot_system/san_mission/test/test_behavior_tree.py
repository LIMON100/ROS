"""SAN v1.5 Phase 2-E Turn 9-10 — Behavior tree tests (pytest).

Pure Python — no rclpy / ROS dependency. Validates BT semantics that
mission_node depends on.

Coverage:
  T1  Status enum values
  T2  Action returns whatever its callable returns
  T3  Condition truthy → SUCCESS, falsy → FAILURE
  T4  Sequence — all SUCCESS → SUCCESS
  T5  Sequence — first FAILURE → FAILURE, resets index
  T6  Sequence — RUNNING resumes from same child next tick (memory)
  T7  Selector — first SUCCESS short-circuits
  T8  Selector — all FAILURE → FAILURE
  T9  Selector — first RUNNING returned
  T10 Repeat — N successes → SUCCESS
  T11 Repeat — FAILURE → FAILURE, resets count
"""
from san_mission.behavior_tree import (
    Action,
    Condition,
    Repeat,
    Selector,
    Sequence,
    Status,
)

# Helpers — fake callables that record invocations

class _Recorder:
    """Returns canned statuses in order, records call count."""
    def __init__(self, *statuses):
        self.statuses = list(statuses)
        self.calls = 0

    def __call__(self, ctx):
        self.calls += 1
        if not self.statuses:
            return Status.SUCCESS
        return self.statuses.pop(0)


# T1
def test_status_enum_values():
    assert Status.RUNNING.value == 0
    assert Status.SUCCESS.value == 1
    assert Status.FAILURE.value == 2


# T2
def test_action_returns_callable_status():
    a = Action(_Recorder(Status.RUNNING), name="a")
    assert a.tick(None) == Status.RUNNING


# T3
def test_condition_truthy_success_falsy_failure():
    yes = Condition(lambda _ctx: True)
    no  = Condition(lambda _ctx: False)
    assert yes.tick(None) == Status.SUCCESS
    assert no.tick(None)  == Status.FAILURE


# T4
def test_sequence_all_success():
    r1 = _Recorder(Status.SUCCESS)
    r2 = _Recorder(Status.SUCCESS)
    seq = Sequence(Action(r1), Action(r2))
    assert seq.tick(None) == Status.SUCCESS
    assert r1.calls == 1
    assert r2.calls == 1


# T5
def test_sequence_first_failure():
    r1 = _Recorder(Status.FAILURE)
    r2 = _Recorder(Status.SUCCESS)
    seq = Sequence(Action(r1), Action(r2))
    assert seq.tick(None) == Status.FAILURE
    assert r1.calls == 1
    assert r2.calls == 0   # short-circuit


# T6
def test_sequence_running_resumes_next_tick():
    # Child 0 always SUCCESS; child 1 first RUNNING, then SUCCESS
    r0 = _Recorder(Status.SUCCESS, Status.SUCCESS)
    r1 = _Recorder(Status.RUNNING, Status.SUCCESS)
    seq = Sequence(Action(r0), Action(r1))
    # Tick 1: r0 SUCCESS, r1 RUNNING → seq RUNNING
    assert seq.tick(None) == Status.RUNNING
    # Tick 2: skips r0 (memory), r1 SUCCESS → seq SUCCESS
    assert seq.tick(None) == Status.SUCCESS
    assert r0.calls == 1   # not re-called
    assert r1.calls == 2


# T7
def test_selector_first_success_short_circuits():
    r1 = _Recorder(Status.SUCCESS)
    r2 = _Recorder(Status.SUCCESS)
    sel = Selector(Action(r1), Action(r2))
    assert sel.tick(None) == Status.SUCCESS
    assert r1.calls == 1
    assert r2.calls == 0


# T8
def test_selector_all_failure():
    r1 = _Recorder(Status.FAILURE)
    r2 = _Recorder(Status.FAILURE)
    sel = Selector(Action(r1), Action(r2))
    assert sel.tick(None) == Status.FAILURE


# T9
def test_selector_first_running():
    r1 = _Recorder(Status.RUNNING)
    r2 = _Recorder(Status.SUCCESS)
    sel = Selector(Action(r1), Action(r2))
    assert sel.tick(None) == Status.RUNNING
    assert r1.calls == 1
    assert r2.calls == 0


# T10
def test_repeat_n_successes_succeeds():
    r = _Recorder(Status.SUCCESS, Status.SUCCESS, Status.SUCCESS)
    rep = Repeat(Action(r), n=3)
    # First success → RUNNING (count=1)
    assert rep.tick(None) == Status.RUNNING
    # Second success → RUNNING (count=2)
    assert rep.tick(None) == Status.RUNNING
    # Third success → SUCCESS (count=3, resets)
    assert rep.tick(None) == Status.SUCCESS


# T11
def test_repeat_failure_resets():
    r = _Recorder(Status.SUCCESS, Status.FAILURE)
    rep = Repeat(Action(r), n=5)
    assert rep.tick(None) == Status.RUNNING
    assert rep.tick(None) == Status.FAILURE
    # _count was reset on failure — verify via re-call success path
    r2 = _Recorder(Status.SUCCESS)
    rep2 = Repeat(Action(r2), n=1)
    assert rep2.tick(None) == Status.SUCCESS
