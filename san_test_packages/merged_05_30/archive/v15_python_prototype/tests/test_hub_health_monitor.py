"""Tests for HubHealthMonitor + ThreatAlert (dual-SBC peer surveillance)."""
import pytest

from core.messages import (
    THREAT_SEVERITY_CRITICAL,
    THREAT_SEVERITY_INFO,
    THREAT_SEVERITY_WARNING,
    THREAT_TYPE_SBC_FAILED,
    ThreatAlert,
)
from safety.hub_health_monitor import HEARTBEAT_TIMEOUT_SEC, HubHealthMonitor

# ─── ThreatAlert validators ─────────────────────────────────────────────

def test_threat_alert_default_validates():
    ThreatAlert().validate()


def test_threat_alert_all_severities_validate():
    for sev in (THREAT_SEVERITY_INFO, THREAT_SEVERITY_WARNING,
                THREAT_SEVERITY_CRITICAL):
        ThreatAlert(severity=sev).validate()


def test_threat_alert_rejects_unknown_severity():
    with pytest.raises(ValueError):
        ThreatAlert(severity="urgent").validate()


def test_threat_alert_rejects_negative_threat_type():
    with pytest.raises(ValueError):
        ThreatAlert(threat_type=-1).validate()


def test_sbc_failed_sentinel_value():
    # Spec pins SBC_FAILED at 99; downstream consumers depend on it.
    assert THREAT_TYPE_SBC_FAILED == 99


# ─── HubHealthMonitor: ingest + tick ────────────────────────────────────

def test_default_timeout_matches_spec():
    assert HEARTBEAT_TIMEOUT_SEC == 3.0


def test_fresh_heartbeat_no_timeout():
    m = HubHealthMonitor(peer_ids=["1", "2"])
    m.record_heartbeat("1", now=10.0)
    m.record_heartbeat("2", now=10.0)
    assert m.check_timeouts(now=12.0) == []   # within 3 s timeout


def test_timeout_fires_after_3s():
    m = HubHealthMonitor(peer_ids=["1", "2"])
    m.record_heartbeat("1", now=10.0)
    m.record_heartbeat("2", now=10.0)
    # 3.0s is the boundary — strictly greater than triggers.
    assert m.check_timeouts(now=13.01) == ["1", "2"]


def test_only_stale_peer_listed():
    m = HubHealthMonitor(peer_ids=["1", "2"])
    m.record_heartbeat("1", now=10.0)
    m.record_heartbeat("2", now=10.0)
    # Refresh peer 2 right before the check; only peer 1 goes stale.
    m.record_heartbeat("2", now=14.0)
    assert m.check_timeouts(now=14.5) == ["1"]


def test_timeout_de_duped_until_recovery():
    m = HubHealthMonitor(peer_ids=["1"])
    m.record_heartbeat("1", now=10.0)
    assert m.check_timeouts(now=14.0) == ["1"]
    # Second consecutive check_timeouts must NOT re-fire while peer is
    # still silent — caller already saw this outage.
    assert m.check_timeouts(now=20.0) == []
    assert m.check_timeouts(now=30.0) == []


def test_recovery_returns_true_and_re_arms_alerting():
    m = HubHealthMonitor(peer_ids=["1"])
    m.record_heartbeat("1", now=10.0)
    m.check_timeouts(now=14.0)             # latched stale
    recovered = m.record_heartbeat("1", now=15.0)
    assert recovered is True
    # Once recovered, a subsequent silent stretch must fire again.
    assert m.check_timeouts(now=19.0) == ["1"]


def test_recovery_returns_false_when_not_previously_stale():
    m = HubHealthMonitor(peer_ids=["1"])
    m.record_heartbeat("1", now=10.0)
    assert m.record_heartbeat("1", now=11.0) is False


def test_unknown_peer_silently_ignored():
    m = HubHealthMonitor(peer_ids=["1"])
    # Unknown peer: record returns False, no exception, no side effects.
    assert m.record_heartbeat("zzz", now=10.0) is False
    assert m.peer_ids() == ["1"]


def test_never_seen_peer_flagged_after_timeout():
    # A peer that never beat at all should still be detected — the spec
    # cares about silent peers, including ones that never came up.
    m = HubHealthMonitor(peer_ids=["1"])
    # now=0 with last_seen_t=0 — the monitor treats "never seen" as
    # infinitely stale, so the first tick past timeout fires.
    assert m.check_timeouts(now=100.0) == ["1"]


def test_custom_timeout_honored():
    m = HubHealthMonitor(peer_ids=["1"], timeout_sec=0.5)
    m.record_heartbeat("1", now=10.0)
    assert m.check_timeouts(now=10.49) == []
    assert m.check_timeouts(now=10.51) == ["1"]


def test_is_timed_out_tracks_state():
    m = HubHealthMonitor(peer_ids=["1"])
    m.record_heartbeat("1", now=10.0)
    assert m.is_timed_out("1") is False
    m.check_timeouts(now=14.0)
    assert m.is_timed_out("1") is True
    m.record_heartbeat("1", now=15.0)
    assert m.is_timed_out("1") is False


def test_empty_roster_never_fires():
    m = HubHealthMonitor(peer_ids=[])
    assert m.check_timeouts(now=10_000.0) == []
