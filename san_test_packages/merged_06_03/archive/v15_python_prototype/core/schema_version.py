"""IDS message schema version negotiation.

The SAN IDS (SAN-IDS-CMD-001) is versioned as `major.minor`. A peer's
*major* identifies the wire-format generation: peers with different
majors are not interoperable and must refuse to connect. *Minor* tracks
additive changes — new message types, new optional fields. A v1.1 peer
talking to a v1.0 peer must downgrade by suppressing v1.1-only messages
on the wire; receiving an unknown message on either side is silently
discarded by DDS, but emitting one to a v1.0 peer wastes bandwidth and
can trip strict validators.

This module is the single source of truth for the local peer's version
and the per-message minor-version gate. Update `SCHEMA_MINOR` and the
`MESSAGE_INTRODUCED_IN_MINOR` table together whenever the IDS grows.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Mapping, Tuple

# Local peer's schema version. Bumped per IDS revision.
SCHEMA_MAJOR: int = 1
SCHEMA_MINOR: int = 1

# Minor version in which each message type was added. Messages absent
# from this table are assumed to be part of the v1.0 baseline and are
# always sent. Keys are the dataclass class names from core.messages —
# kept as strings to avoid an import cycle when this module is read
# from inside a message dataclass __post_init__.
MESSAGE_INTRODUCED_IN_MINOR: Mapping[str, int] = {
    # v1.1 additions (SAN-IDS-CMD-001 v1.1 §3.9, §5.5–5.11)
    "VideoRequest":     1,   # §3.9  Tablet → Hub video pull
    "SectorAssign":     1,   # §5.10 Leader → robot surveillance sector
    "PanTiltCommand":   1,   # §5.11 Robot pan-tilt control
    "AggregatedMap":    1,   # §5.6  Hub broadcast 30–60 s
    "SLAMLocalDelta":   1,   # §5.5  Follower 30–60 s delta (replaces SLAMDelta)
    # VideoResponse field extension (`srt_uri`) lives at the same minor;
    # callers reading `srt_uri` must already require v1.1 on the peer.
}


@dataclass(frozen=True)
class SchemaVersion:
    """Schema version pair, sortable + serialisable."""
    major: int
    minor: int

    def as_tuple(self) -> Tuple[int, int]:
        return (self.major, self.minor)

    def __str__(self) -> str:  # for log lines
        return f"{self.major}.{self.minor}"


LOCAL_VERSION = SchemaVersion(SCHEMA_MAJOR, SCHEMA_MINOR)


def is_compatible(peer_major: int, peer_minor: int) -> bool:
    """True iff the local peer can exchange messages with the peer.

    Compatibility is *major-equal only* — a v2 peer cannot talk to v1
    without an explicit bridge. The minor difference is not a
    compatibility failure; it only gates which messages the local peer
    is allowed to emit (see `supports_message`).
    """
    return int(peer_major) == SCHEMA_MAJOR


def supports_message(
    message_name: str,
    peer_major: int,
    peer_minor: int,
) -> bool:
    """True iff `message_name` can be safely emitted toward this peer.

    A v1.0 peer receiving a v1.1-only message would either drop it
    silently (best case) or raise a deserialise error in a strict
    validator (worst case). Call this before publishing on any topic
    whose message is listed in MESSAGE_INTRODUCED_IN_MINOR.

    Messages not in the table are baseline (v1.0) and always supported
    when `is_compatible` returns True.
    """
    if not is_compatible(peer_major, peer_minor):
        return False
    introduced = MESSAGE_INTRODUCED_IN_MINOR.get(message_name, 0)
    return int(peer_minor) >= introduced


def negotiate(peer_major: int, peer_minor: int) -> SchemaVersion:
    """Return the highest schema version both peers can speak.

    Raises ValueError when the majors disagree — callers should refuse
    to bring the link up rather than guess at a bridge.
    """
    if not is_compatible(peer_major, peer_minor):
        raise ValueError(
            f"incompatible schema major: local={SCHEMA_MAJOR}, "
            f"peer={peer_major}.{peer_minor}")
    return SchemaVersion(SCHEMA_MAJOR, min(SCHEMA_MINOR, int(peer_minor)))


__all__ = (
    "LOCAL_VERSION",
    "MESSAGE_INTRODUCED_IN_MINOR",
    "SCHEMA_MAJOR",
    "SCHEMA_MINOR",
    "SchemaVersion",
    "is_compatible",
    "negotiate",
    "supports_message",
)
