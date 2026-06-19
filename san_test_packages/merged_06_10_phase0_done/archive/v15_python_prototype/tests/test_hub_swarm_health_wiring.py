"""Tests for the HubUgvAdapter SwarmHealthSummary wiring (gap #1).

These exercise only the role-state machine and the publish helper —
the BaseProcess spawn path is intentionally bypassed via __new__ +
manual attribute injection, matching the pattern in test_hub_ugv.py.
"""
from __future__ import annotations

import multiprocessing as mp
import queue as q
import threading
from typing import Dict
from unittest.mock import MagicMock

from adapters.hub_ugv import HubUgvAdapter
from core.messages import SwarmHealthSummary
from safety.hub_health_monitor import HubHealthMonitor


def _make_hub_with_health(peer_roles: Dict[str, str]):
    """Build a Hub adapter wired for the SwarmHealthSummary path.

    `peer_roles` maps peer_id → 'slam'|'comm'. Constructs a real
    HubHealthMonitor over the same peer_ids so tests can drive
    record_heartbeat / check_timeouts realistically.
    """
    h = HubUgvAdapter.__new__(HubUgvAdapter)
    h.queues = MagicMock()
    h.queues.hub_swarm_health_summary = mp.Queue(maxsize=8)
    h.queues.hub_threat_alert = mp.Queue(maxsize=8)
    h.cfg = MagicMock()
    h.log = MagicMock()
    h.role = "hub"
    h._lock = threading.Lock()
    h._follower_tiles = {}
    h._is_acting_leader = False
    h._last_leader_seen = 0.0
    h._stats = {
        "follower_maps_in": 0, "shared_maps_out": 0,
        "leader_takeovers": 0, "aggregated_maps_out": 0,
        "peer_sbc_failures": 0, "peer_sbc_recoveries": 0,
        "swarm_health_summaries_out": 0,
    }
    h._peer_sbc_role = dict(peer_roles)
    h._sbc_failed = {"slam": False, "comm": False}
    h._sbc_failed_peer = {"slam": "", "comm": ""}
    h._sbc_state_lock = threading.Lock()
    h._hub_health = HubHealthMonitor(
        peer_ids=list(peer_roles.keys()), timeout_sec=3.0)
    return h


def _drain_summary(h) -> SwarmHealthSummary:
    """Pop one SwarmHealthSummary from the queue (raises if empty)."""
    return h.queues.hub_swarm_health_summary.get(timeout=0.5)


def _queue_empty(h) -> bool:
    try:
        h.queues.hub_swarm_health_summary.get_nowait()
    except q.Empty:
        return True
    return False


# ─── role mapping ──────────────────────────────────────────────────────

def test_role_for_peer_returns_mapped_role():
    h = _make_hub_with_health({"sbc1": "slam", "sbc2": "comm"})
    assert h._role_for_peer("sbc1") == "slam"
    assert h._role_for_peer("sbc2") == "comm"


def test_role_for_peer_unmapped_returns_empty():
    h = _make_hub_with_health({"sbc1": "slam"})
    assert h._role_for_peer("sbc99") == ""


# ─── failure latch + publish ───────────────────────────────────────────

def test_slam_timeout_publishes_summary_with_slam_flag():
    h = _make_hub_with_health({"sbc1": "slam", "sbc2": "comm"})
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    msg = _drain_summary(h)
    assert msg.slam_sbc_failed is True
    assert msg.comm_sbc_failed is False
    assert msg.slam_sbc_peer_id == "sbc1"
    assert msg.comm_sbc_peer_id == ""
    assert h._stats["swarm_health_summaries_out"] == 1


def test_comm_timeout_publishes_summary_with_comm_flag():
    h = _make_hub_with_health({"sbc1": "slam", "sbc2": "comm"})
    h._update_sbc_role_state(peer_id="sbc2", failed=True)
    msg = _drain_summary(h)
    assert msg.slam_sbc_failed is False
    assert msg.comm_sbc_failed is True
    assert msg.comm_sbc_peer_id == "sbc2"


def test_both_sbcs_failed_carries_both_flags():
    h = _make_hub_with_health({"sbc1": "slam", "sbc2": "comm"})
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    _drain_summary(h)
    h._update_sbc_role_state(peer_id="sbc2", failed=True)
    msg = _drain_summary(h)
    # Second publish reflects accumulated state from both transitions.
    assert msg.slam_sbc_failed is True
    assert msg.comm_sbc_failed is True
    assert msg.slam_sbc_peer_id == "sbc1"
    assert msg.comm_sbc_peer_id == "sbc2"


def test_recovery_clears_role_flag_and_publishes():
    h = _make_hub_with_health({"sbc1": "slam"})
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    _drain_summary(h)
    h._update_sbc_role_state(peer_id="sbc1", failed=False)
    msg = _drain_summary(h)
    assert msg.slam_sbc_failed is False
    assert msg.slam_sbc_peer_id == ""
    assert h._stats["swarm_health_summaries_out"] == 2


def test_idempotent_no_transition_no_publish():
    # Setting the same flag twice must not flood the queue.
    h = _make_hub_with_health({"sbc1": "slam"})
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    _drain_summary(h)
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    assert _queue_empty(h)
    assert h._stats["swarm_health_summaries_out"] == 1


def test_recovery_on_already_clear_state_no_publish():
    h = _make_hub_with_health({"sbc1": "slam"})
    # Adapter starts with everything clear; a recovery for an already-
    # clear role is a no-op.
    h._update_sbc_role_state(peer_id="sbc1", failed=False)
    assert _queue_empty(h)
    assert h._stats["swarm_health_summaries_out"] == 0


def test_unmapped_peer_no_summary():
    # No role mapping for sbc99 — adapter must not publish a
    # SwarmHealthSummary (the operator still gets the ThreatAlert via
    # the unchanged existing path).
    h = _make_hub_with_health({"sbc1": "slam"})
    h._update_sbc_role_state(peer_id="sbc99", failed=True)
    assert _queue_empty(h)
    assert h._stats["swarm_health_summaries_out"] == 0


def test_invalid_role_in_config_logged_and_ignored():
    # An adapter that wasn't built via setup() can't exercise the
    # config-parsing log path, but we can verify the role-lookup
    # rejects an unmapped role gracefully.
    h = _make_hub_with_health({})
    h._peer_sbc_role["sbc-typo"] = "slamm"
    # Direct lookup returns the raw stored value — the publish helper
    # filters by the {slam, comm} whitelist.
    h._update_sbc_role_state(peer_id="sbc-typo", failed=True)
    assert _queue_empty(h)


# ─── recovery path via record_peer_heartbeat ───────────────────────────

def test_recovery_via_record_peer_heartbeat_clears_summary():
    h = _make_hub_with_health({"sbc1": "slam"})
    # Simulate a timeout via the watchdog state directly.
    h._update_sbc_role_state(peer_id="sbc1", failed=True)
    _drain_summary(h)
    # Manually expire the heartbeat tracker so record_heartbeat sees
    # `recovered=True`. (HubHealthMonitor exposes record + check; the
    # easiest way to get a "previously timed out" state is to drive
    # check_timeouts past the boundary first.)
    h._hub_health.check_timeouts(now=1_000.0)
    h.record_peer_heartbeat("sbc1", now=1_001.0)
    msg = _drain_summary(h)
    assert msg.slam_sbc_failed is False
    assert msg.slam_sbc_peer_id == ""
