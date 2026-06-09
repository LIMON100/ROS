"""Tests for core.schema_version — IDS version negotiation."""
import pytest

from core.schema_version import (
    LOCAL_VERSION,
    MESSAGE_INTRODUCED_IN_MINOR,
    SCHEMA_MAJOR,
    SCHEMA_MINOR,
    SchemaVersion,
    is_compatible,
    negotiate,
    supports_message,
)

# ─── constants + LOCAL_VERSION ─────────────────────────────────────────

def test_local_version_matches_constants():
    assert LOCAL_VERSION.major == SCHEMA_MAJOR
    assert LOCAL_VERSION.minor == SCHEMA_MINOR


def test_local_version_at_or_above_one_one():
    # IDS v1.1 introduced surveillance + pantilt + SLAMLocalDelta. The
    # codebase must not regress below this.
    assert LOCAL_VERSION.as_tuple() >= (1, 1)


def test_schema_version_str_form():
    assert str(SchemaVersion(1, 1)) == "1.1"


# ─── is_compatible ─────────────────────────────────────────────────────

def test_is_compatible_same_major():
    assert is_compatible(SCHEMA_MAJOR, SCHEMA_MINOR) is True


def test_is_compatible_lower_minor_ok():
    # v1.1 ↔ v1.0 peers can still talk; minor-skew is fine.
    assert is_compatible(SCHEMA_MAJOR, 0) is True


def test_is_compatible_higher_minor_ok():
    # Future v1.2 peer is still compatible — we'll downgrade outbound.
    assert is_compatible(SCHEMA_MAJOR, SCHEMA_MINOR + 99) is True


def test_is_compatible_major_mismatch_rejected():
    assert is_compatible(SCHEMA_MAJOR + 1, 0) is False
    assert is_compatible(SCHEMA_MAJOR - 1, 0) is False


# ─── supports_message ──────────────────────────────────────────────────

def test_supports_baseline_message_on_v1_0_peer():
    # Baseline messages (not in the introduced-in table) are always OK.
    assert supports_message("RobotStatus", SCHEMA_MAJOR, 0) is True


def test_supports_v1_1_message_on_v1_1_peer():
    assert supports_message("PanTiltCommand", SCHEMA_MAJOR, 1) is True


def test_supports_v1_1_message_on_v1_0_peer_blocked():
    # PanTiltCommand was added in v1.1 — must not be emitted to a v1.0 peer.
    assert supports_message("PanTiltCommand", SCHEMA_MAJOR, 0) is False
    assert supports_message("SectorAssign", SCHEMA_MAJOR, 0) is False
    assert supports_message("SLAMLocalDelta", SCHEMA_MAJOR, 0) is False


def test_supports_any_message_blocked_on_major_mismatch():
    # Major mismatch = link refused; supports_message is short-circuited.
    assert supports_message("RobotStatus", SCHEMA_MAJOR + 1, 0) is False
    assert supports_message("PanTiltCommand", SCHEMA_MAJOR + 1, 1) is False


def test_introduced_table_covers_v1_1_additions():
    # README-1.md lists these as v1.1 additions; the table is the wire
    # contract — anything missing here ships freely to a v1.0 peer.
    for name in ("VideoRequest", "SectorAssign", "PanTiltCommand",
                 "AggregatedMap", "SLAMLocalDelta"):
        assert name in MESSAGE_INTRODUCED_IN_MINOR
        assert MESSAGE_INTRODUCED_IN_MINOR[name] == 1


# ─── negotiate ─────────────────────────────────────────────────────────

def test_negotiate_picks_lower_minor():
    # Local 1.1 ↔ peer 1.0 → effective 1.0.
    v = negotiate(SCHEMA_MAJOR, 0)
    assert v == SchemaVersion(SCHEMA_MAJOR, 0)


def test_negotiate_same_version_returns_local():
    v = negotiate(SCHEMA_MAJOR, SCHEMA_MINOR)
    assert v == LOCAL_VERSION


def test_negotiate_caps_at_local_minor():
    # Future peer at 1.99 — we still cap at our local SCHEMA_MINOR.
    v = negotiate(SCHEMA_MAJOR, SCHEMA_MINOR + 99)
    assert v == LOCAL_VERSION


def test_negotiate_raises_on_major_mismatch():
    with pytest.raises(ValueError):
        negotiate(SCHEMA_MAJOR + 1, 0)
