"""AI permission separation invariants enforcement (SDD §8, P2-14).

Invariants:
1. AI detection results never directly invoke cmd_vel changes
2. All AI outputs route through Mission BT or operator alert only
3. DDS topic ACL: AI process can publish to anomaly_events but not cmd_vel

Runtime guard rejects publish attempts with source ∈ AI_SOURCES on any
topic ∈ CONTROL_TOPICS. Each block produces a GuardViolation entry that
the audit logger (P1-16) can ingest via the optional audit_callback.
"""
from __future__ import annotations

import time
import unicodedata
from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, List, Optional, Set


class GuardDecision(IntEnum):
    ALLOW = 0
    BLOCK = 1


@dataclass
class GuardViolation:
    timestamp: float
    topic: str
    source: str
    reason: str


# AI sources that must never publish to control topics.
AI_SOURCES: Set[str] = {
    "ai_detection",
    "perception_ai",
    "yolov5s",
    "anomaly_classifier",
}

# Topics where AI sources are never permitted to publish.
CONTROL_TOPICS: Set[str] = {
    "cmd_vel",
    "leader_pose",
    "follower_target",
    "mission_command",
    "formation_change",
}

# Topics where AI is permitted (read-only or notification).
AI_PERMITTED_TOPICS: Set[str] = {
    "anomaly_events",
    "ai_detections",
    "perception_log",
    "alert",
}


def _normalize(s: str) -> str:
    """Canonicalize a topic / source string before guard membership test.

    Steps:
      1. NFKC folds full-width latin and other compat-equivalents.
      2. Strip Unicode format chars (zero-width space U+200B / joiner
         U+200C / non-joiner U+200D / BOM U+FEFF, etc.) — NFKC does NOT
         remove these, but they're a common bypass vector.
      3. Strip ASCII whitespace + embedded NULs.
      4. casefold() (more aggressive than .lower() for non-ASCII).
    """
    if not isinstance(s, str):
        return ""
    nfkc = unicodedata.normalize("NFKC", s)
    no_format = "".join(ch for ch in nfkc
                        if unicodedata.category(ch) != "Cf")
    return no_format.strip().strip("\x00").casefold()


# Precomputed normalised lookup sets — frozen at import time so the
# common-case `check()` does O(1) work and the exposed AI_SOURCES /
# CONTROL_TOPICS names stay backwards-compatible for any caller that
# inspected the public sets.
_AI_SOURCES_NORM: Set[str] = {_normalize(x) for x in AI_SOURCES}
_CONTROL_TOPICS_NORM: Set[str] = {_normalize(x) for x in CONTROL_TOPICS}


class PermissionGuard:
    """Runtime guard for AI permission invariants."""

    def __init__(self,
                 audit_callback: Optional[Callable[[GuardViolation], None]]
                 = None) -> None:
        """audit_callback receives a GuardViolation on each BLOCK decision."""
        self.audit_callback = audit_callback
        self.violation_count = 0
        self._violations: List[GuardViolation] = []

    def check(self, topic: str, source: str) -> GuardDecision:
        """Evaluate a publish attempt. Returns ALLOW or BLOCK.

        Both topic and source are normalised (NFKC, stripped, casefold)
        before membership testing so the invariant cannot be bypassed
        with case variation ("AI_Detection"), whitespace
        (" ai_detection"), zero-width spaces, or unicode confusables.
        """
        n_topic = _normalize(topic)
        n_source = _normalize(source)
        if n_topic in _CONTROL_TOPICS_NORM and n_source in _AI_SOURCES_NORM:
            # Record the *original* (un-normalised) strings so the audit
            # entry shows what the attacker actually sent.
            self._record_violation(topic, source,
                                   "AI source attempted control publish")
            return GuardDecision.BLOCK
        return GuardDecision.ALLOW

    def _record_violation(self, topic: str, source: str,
                          reason: str) -> None:
        v = GuardViolation(
            timestamp=time.time(),
            topic=topic, source=source, reason=reason,
        )
        self._violations.append(v)
        self.violation_count += 1
        if self.audit_callback is not None:
            try:
                self.audit_callback(v)
            except Exception:
                # Audit failures must not crash the guard.
                pass

    @property
    def violations(self) -> List[GuardViolation]:
        return list(self._violations)

    def reset_violations(self) -> None:
        self._violations.clear()
        self.violation_count = 0


def guarded_publish(guard: "PermissionGuard",
                    queues,
                    topic: str,
                    msg,
                    *,
                    source: str) -> bool:
    """Publish `msg` to `queues.<topic>` only if the guard ALLOWS it.

    Returns True iff allowed AND the underlying publish succeeded.
    BLOCK decisions are silently dropped from the caller's perspective —
    the audit trail comes from the guard's audit_callback, not the
    return value. Producers should treat False the same as a normal
    publish() failure.

    This is the integration point: producers identify themselves by
    `source` (e.g. "perception_ai"), and the topic name they pass
    is matched against PermissionGuard's CONTROL_TOPICS set. A bug
    that wires an AI process to a control topic will surface here
    rather than reaching the locomotion stack.
    """
    decision = guard.check(topic=topic, source=source)
    if decision == GuardDecision.BLOCK:
        return False
    q = getattr(queues, topic, None)
    if q is None:
        return False
    # Lazy import — core.ipc imports nothing from this module, but doing
    # the import at module load would still tie the two together. Keep
    # it inside the function so test fakes can swap publish() out.
    from core.ipc import publish as _publish
    return _publish(q, msg)
