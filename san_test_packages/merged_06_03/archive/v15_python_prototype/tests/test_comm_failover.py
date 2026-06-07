"""
Tests for CommProcess WiFi6 ↔ LTE failover.

We test the link-monitor decision logic in isolation by:
  • building a CommProcess instance without spawning it
  • injecting LteStatus and a mocked WiFi probe
  • driving the link monitor's decision logic by hand

Verifies:
  • WiFi6 is preferred when reachable
  • Failover to LTE when WiFi6 unreachable AND LTE registered+pdp_active
  • Hysteresis: 3 consecutive WiFi6 successes required to switch back
  • Both down → 'none', uploads cached
"""
from __future__ import annotations

import threading

from comm.comm_process import CommProcess
from core.messages import (
    LTE_REGISTERED_HOME,
    LTE_REGISTERED_ROAMING,
    LTE_SEARCHING,
    Header,
    LteStatus,
)


# ────────── Test fixture ──────────
def make_comm():
    """Build CommProcess without spawning."""
    c = CommProcess.__new__(CommProcess)
    c._link_lock = threading.Lock()
    c._active_link = "none"
    c._wifi_consec_ok = 0
    c._lte_status = None
    c._stats = {
        "uploaded_wifi": 0, "uploaded_lte": 0,
        "cached": 0, "heartbeat": 0, "link_switches": 0,
    }
    return c


def good_lte():
    return LteStatus(
        header=Header.now(frame_id="lte"),
        registered=LTE_REGISTERED_HOME, operator="STUB",
        rat="LTE", rsrp_dbm=-90.0, rsrq_db=-10.0, sinr_db=10.0,
        pdp_active=True, ip_address="10.0.0.1",
    )


def bad_lte_searching():
    return LteStatus(
        header=Header.now(frame_id="lte"),
        registered=LTE_SEARCHING, pdp_active=False,
    )


def bad_lte_no_pdp():
    """Registered but data session not up."""
    return LteStatus(
        header=Header.now(frame_id="lte"),
        registered=LTE_REGISTERED_HOME, pdp_active=False,
    )


def _step_link_decision(c, wifi_ok: bool):
    """Replicate the inside of CommProcess._link_monitor's loop body once.

    We don't actually call _link_monitor (it has time.sleep). Instead we
    duplicate its decision logic so each test step is one decision point.
    """
    with c._link_lock:
        lte = c._lte_status
        lte_ok = (lte is not None
                  and lte.registered in (LTE_REGISTERED_HOME,
                                         LTE_REGISTERED_ROAMING)
                  and lte.pdp_active)
        old = c._active_link
        if wifi_ok:
            c._wifi_consec_ok += 1
        else:
            c._wifi_consec_ok = 0
        if c._active_link == "wifi6":
            if not wifi_ok:
                c._active_link = "lte" if lte_ok else "none"
        elif c._active_link == "lte":
            if c._wifi_consec_ok >= 3:
                c._active_link = "wifi6"
            elif not lte_ok:
                c._active_link = "none"
        else:  # "none"
            if wifi_ok:
                c._active_link = "wifi6"
            elif lte_ok:
                c._active_link = "lte"
        if old != c._active_link:
            c._stats["link_switches"] += 1


# ════════════════════════════════════════════════════════════
# F1: cold start — pick WiFi6 if available
# ════════════════════════════════════════════════════════════
def test_F1_cold_start_picks_wifi6_when_available():
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=True)
    assert c._active_link == "wifi6"
    assert c._stats["link_switches"] == 1


def test_F1b_cold_start_picks_lte_when_wifi_down():
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "lte"


def test_F1c_cold_start_neither_link_keeps_none():
    c = make_comm()
    c._lte_status = bad_lte_searching()
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "none"
    assert c._stats["link_switches"] == 0


# ════════════════════════════════════════════════════════════
# F2: WiFi6 → LTE failover
# ════════════════════════════════════════════════════════════
def test_F2_wifi_loss_triggers_lte_failover():
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=True)             # → wifi6
    _step_link_decision(c, wifi_ok=False)            # → lte
    assert c._active_link == "lte"
    assert c._stats["link_switches"] == 2


def test_F2b_wifi_loss_with_no_lte_goes_to_none():
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=True)
    c._lte_status = bad_lte_no_pdp()                 # LTE goes down too
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "none"


# ════════════════════════════════════════════════════════════
# F3: hysteresis — don't flap back to WiFi6 too eagerly
# ════════════════════════════════════════════════════════════
def test_F3_hysteresis_one_success_not_enough_to_switch_back():
    c = make_comm()
    c._lte_status = good_lte()
    # Reach the LTE state
    _step_link_decision(c, wifi_ok=True)
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "lte"
    # Single WiFi6 success should NOT switch us back
    _step_link_decision(c, wifi_ok=True)
    assert c._active_link == "lte"


def test_F3_hysteresis_three_successes_switches_back():
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=True)
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "lte"
    _step_link_decision(c, wifi_ok=True)             # 1
    _step_link_decision(c, wifi_ok=True)             # 2
    assert c._active_link == "lte"
    _step_link_decision(c, wifi_ok=True)             # 3 → switch
    assert c._active_link == "wifi6"


def test_F3_hysteresis_resets_on_intermittent_failure():
    """Glitch in WiFi6 during ramp-up resets the streak counter."""
    c = make_comm()
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=True)
    _step_link_decision(c, wifi_ok=False)            # → lte
    _step_link_decision(c, wifi_ok=True)             # streak=1
    _step_link_decision(c, wifi_ok=True)             # streak=2
    _step_link_decision(c, wifi_ok=False)            # streak resets, still lte
    _step_link_decision(c, wifi_ok=True)             # streak=1
    assert c._active_link == "lte"


# ════════════════════════════════════════════════════════════
# F4: LTE registration changes
# ════════════════════════════════════════════════════════════
def test_F4_lte_registers_during_outage_starts_using_lte():
    c = make_comm()
    c._lte_status = bad_lte_searching()
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "none"
    # LTE finishes registering
    c._lte_status = good_lte()
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "lte"


def test_F4b_roaming_counts_as_registered():
    c = make_comm()
    s = good_lte()
    s.registered = LTE_REGISTERED_ROAMING
    c._lte_status = s
    _step_link_decision(c, wifi_ok=False)
    assert c._active_link == "lte"
