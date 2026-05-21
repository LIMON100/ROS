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


class Sequence(Node):
    """Run children in order. Fail on first failure, run on first
    running, succeed when all succeed. Resumes from last running
    child on next tick (memory semantic)."""

    def __init__(self, *children: Node, name: str = "Sequence"):
        super().__init__(name)
        self.children = list(children)
        self._idx = 0

    def tick(self, ctx) -> Status:
        while self._idx < len(self.children):
            s = self.children[self._idx].tick(ctx)
            if s == Status.RUNNING:
                return Status.RUNNING
            if s == Status.FAILURE:
                self._idx = 0
                return Status.FAILURE
            self._idx += 1
        self._idx = 0
        return Status.SUCCESS


class Selector(Node):
    """Run children in order. Succeed on first non-failure (running or
    success). Fail only when all fail."""

    def __init__(self, *children: Node, name: str = "Selector"):
        super().__init__(name)
        self.children = list(children)

    def tick(self, ctx) -> Status:
        for c in self.children:
            s = c.tick(ctx)
            if s in (Status.RUNNING, Status.SUCCESS):
                return s
        return Status.FAILURE


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
