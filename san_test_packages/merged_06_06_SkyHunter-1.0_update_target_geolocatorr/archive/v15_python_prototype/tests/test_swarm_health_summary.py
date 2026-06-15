"""Tests for the SwarmHealthSummary message (gap #1 of HubHealthMonitor)."""
import pytest

from core.messages import SwarmHealthSummary


def test_defaults_validate():
    SwarmHealthSummary().validate()


def test_slam_failed_with_peer_id_validates():
    SwarmHealthSummary(
        slam_sbc_failed=True, slam_sbc_peer_id="sbc1",
        timestamp_ms=1_700_000_000_000,
    ).validate()


def test_comm_failed_with_peer_id_validates():
    SwarmHealthSummary(
        comm_sbc_failed=True, comm_sbc_peer_id="sbc2",
        timestamp_ms=1_700_000_000_000,
    ).validate()


def test_both_failed_validates():
    SwarmHealthSummary(
        slam_sbc_failed=True, slam_sbc_peer_id="sbc1",
        comm_sbc_failed=True, comm_sbc_peer_id="sbc2",
    ).validate()


def test_slam_failed_without_peer_id_rejected():
    with pytest.raises(ValueError):
        SwarmHealthSummary(slam_sbc_failed=True).validate()


def test_comm_failed_without_peer_id_rejected():
    with pytest.raises(ValueError):
        SwarmHealthSummary(comm_sbc_failed=True).validate()


def test_negative_timestamp_rejected():
    with pytest.raises(ValueError):
        SwarmHealthSummary(timestamp_ms=-1).validate()


def test_peer_ids_set_without_failure_flag_is_fine():
    # Empty peer_id when flag is False is the normal cleared state. A
    # non-empty peer_id with flag=False is unusual (a recovery's
    # leftover) but not invalid — the validator only requires peer_id
    # when the flag is True.
    SwarmHealthSummary(
        slam_sbc_failed=False, slam_sbc_peer_id="leftover",
    ).validate()
