# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
SAN v1.5 Phase 2-E Turn 9-10 — Behavior tree primitives (pure Python).

Direct port of mission/behavior_tree.py from the legacy Python prototype
to the new san_mission rclpy package per DCN-2026-002 D-007 (Tier 2
rclpy allowed).

No ROS dependencies — fully pytest-able standalone.

Status enum:
  RUNNING — keep ticking
  SUCCESS — done
  FAILURE — abort / retry per parent

Composites:
  Sequence  — children in order, fail on first failure
  Selector  — children in order, succeed on first success

Leaves:
  Action    — does work, returns status
  Condition — pure check (success if truthy, else failure)
  Repeat    — repeat child until N successes or first failure

PATCH 2026-05-13 (san_mission deep-dive review):
  * Sequence and Repeat now expose a reset() method so the BT can be
    re-evaluated cleanly after structural changes (mission restart,
    priority promotion).
  * Sequence accepts memory=False to opt out of resuming from the
    last RUNNING child. SDD §6.1 priority subtrees should NOT have
    memory — they must re-check their Condition every tick so a
    cleared higher-priority precondition (e.g. emergency_active
    going False) drops the subtree at the next tick.
  * Selector inherits reset() that recursively resets composite
    children — necessary when the active priority switches.
"""
from __future__ import annotations

from enum import Enum
from typing import Callable, Optional


class Status(Enum):
    RUNNING = 0
    SUCCESS = 1
    FAILURE = 2


class Node:
    def __init__(self, name: str = ""):
        self.name = name or self.__class__.__name__

    def tick(self, ctx) -> Status:
        ...

    def reset(self) -> None:
        """Reset any internal state (override in composites)."""
        return


class Sequence(Node):
    """Run children in order. Fail on first failure, run on first
    running, succeed when all succeed.

    memory (default True): resume from the last RUNNING child on the
    next tick.

    PATCH 2026-05-13: For SDD §6.1 priority subtrees, pass memory=False
    so the Condition gate is re-evaluated every tick. Without that,
    a P0 emergency that's been cleared externally can leave a child
    looping in RUNNING and starve P1+ subtrees.
    """

    def __init__(self, *children: Node, name: str = "Sequence",
                  memory: bool = True):
        super().__init__(name)
        self.children = list(children)
        self.memory = memory
        self._idx = 0

    def tick(self, ctx) -> Status:
        # PATCH 2026-05-13: when memory is disabled, always start
        # fresh — re-checks Conditions every tick.
        start_idx = self._idx if self.memory else 0
        i = start_idx
        while i < len(self.children):
            s = self.children[i].tick(ctx)
            if s == Status.RUNNING:
                if self.memory:
                    self._idx = i
                else:
                    self._idx = 0
                return Status.RUNNING
            if s == Status.FAILURE:
                # PATCH 2026-05-13: on FAILURE, reset our index AND
                # reset any composite children that may have been
                # mid-flight (they will start fresh next tick too).
                self._idx = 0
                self._reset_children_after(0)
                return Status.FAILURE
            i += 1
        self._idx = 0
        return Status.SUCCESS

    def reset(self) -> None:
        self._idx = 0
        for c in self.children:
            c.reset()

    def _reset_children_after(self, idx: int) -> None:
        for c in self.children[idx:]:
            c.reset()


class Selector(Node):
    """Run children in order. Succeed on first non-failure (running or
    success). Fail only when all fail.

    PATCH 2026-05-13: when the active child changes (priority
    switches in the SDD §6.1 fallback root), previously-active
    composite children are reset so they don't carry over state
    into the next time they're selected.
    """

    def __init__(self, *children: Node, name: str = "Selector"):
        super().__init__(name)
        self.children = list(children)
        self._last_active = -1

    def tick(self, ctx) -> Status:
        for i, c in enumerate(self.children):
            s = c.tick(ctx)
            if s in (Status.RUNNING, Status.SUCCESS):
                # PATCH 2026-05-13: if the active child changed,
                # reset the previously-active one so it doesn't
                # cling to stale memory next time.
                if self._last_active != i and self._last_active >= 0:
                    self.children[self._last_active].reset()
                self._last_active = i
                return s
        # All FAILURE — clear active marker too.
        if self._last_active >= 0:
            self.children[self._last_active].reset()
            self._last_active = -1
        return Status.FAILURE

    def reset(self) -> None:
        self._last_active = -1
        for c in self.children:
            c.reset()


class Action(Node):
    """Wraps a callable that returns a Status."""

    def __init__(self, fn: Callable, name: str = "Action"):
        super().__init__(name)
        self.fn = fn

    def tick(self, ctx) -> Status:
        return self.fn(ctx)


class Condition(Node):
    """Wraps a predicate. Returns SUCCESS if predicate truthy else
    FAILURE. Always one-shot (never RUNNING)."""

    def __init__(self, fn: Callable, name: str = "Condition"):
        super().__init__(name)
        self.fn = fn

    def tick(self, ctx) -> Status:
        return Status.SUCCESS if self.fn(ctx) else Status.FAILURE


class Repeat(Node):
    """Repeat child until it returns FAILURE or N successes."""

    def __init__(self, child: Node, n: Optional[int] = None,
                 name: str = "Repeat"):
        super().__init__(name)
        self.child = child
        self.n = n
        self._count = 0

    def tick(self, ctx) -> Status:
        s = self.child.tick(ctx)
        if s == Status.RUNNING:
            return Status.RUNNING
        if s == Status.FAILURE:
            self._count = 0
            return Status.FAILURE
        # SUCCESS
        self._count += 1
        if self.n is not None and self._count >= self.n:
            self._count = 0
            return Status.SUCCESS
        return Status.RUNNING

    def reset(self) -> None:
        self._count = 0
        self.child.reset()
