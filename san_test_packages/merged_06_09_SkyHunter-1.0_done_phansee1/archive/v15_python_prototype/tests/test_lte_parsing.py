"""
Tests for LTE AT-command response parsing.

Pure function tests on the parsers in adapters.lte_modem — no serial.
"""
from __future__ import annotations

import pytest

from adapters.lte_modem import (
    parse_cesq,
    parse_cgpaddr,
    parse_cops,
    parse_creg,
    parse_qcsq,
)
from core.messages import (
    LTE_REGISTERED_HOME,
    LTE_REGISTERED_ROAMING,
    LTE_SEARCHING,
)


# ───────────── +CREG (registration) ─────────────
def test_creg_home():
    assert parse_creg("+CREG: 0,1") == LTE_REGISTERED_HOME


def test_creg_roaming():
    assert parse_creg("+CREG: 0,5") == LTE_REGISTERED_ROAMING


def test_creg_searching():
    assert parse_creg("+CREG: 0,2") == LTE_SEARCHING


def test_creg_with_lac_ci_extra_fields():
    """+CREG: <n>,<stat>,<lac>,<ci>[,<AcT>] — must still parse."""
    assert parse_creg('+CREG: 2,1,"0001","0F2A",7') == LTE_REGISTERED_HOME


def test_creg_invalid_returns_none():
    assert parse_creg("OK") is None
    assert parse_creg("") is None


# ───────────── +COPS (operator) ─────────────
def test_cops_extracts_operator():
    assert parse_cops('+COPS: 0,0,"KT",7') == "KT"


def test_cops_handles_quoted_operator_with_spaces():
    assert parse_cops('+COPS: 0,0,"SK Telecom",7') == "SK Telecom"


def test_cops_returns_none_when_unregistered():
    assert parse_cops("+COPS: 0") is None


# ───────────── +CESQ (3GPP signal quality) ─────────────
def test_cesq_extracts_rsrp_rsrq():
    # +CESQ: rxlev,ber,rscp,ecno,rsrq,rsrp
    # rsrq=20 → -20 + 20/2 = -10 dB
    # rsrp=70 → -140 + 70 = -70 dBm
    rsrp, rsrq = parse_cesq("+CESQ: 99,99,255,255,20,70")
    assert rsrp == pytest.approx(-70.0, abs=0.1)
    assert rsrq == pytest.approx(-10.0, abs=0.1)


def test_cesq_invalid_indices_yield_floor():
    rsrp, rsrq = parse_cesq("+CESQ: 99,99,255,255,255,255")
    assert rsrp == -140.0
    assert rsrq == -20.0


def test_cesq_returns_none_for_non_cesq():
    assert parse_cesq("OK") is None


# ───────────── +QCSQ (Quectel proprietary) ─────────────
def test_qcsq_lte_with_full_metrics():
    # +QCSQ: "LTE",rssi,rsrp,sinr,rsrq
    rsrp, sinr, rsrq = parse_qcsq('+QCSQ: "LTE",-65,-90,15,-10')
    assert rsrp == pytest.approx(-90.0)
    assert sinr == pytest.approx(15.0)
    assert rsrq == pytest.approx(-10.0)


def test_qcsq_returns_none_when_no_service():
    assert parse_qcsq('+QCSQ: "NOSERVICE"') is None


# ───────────── +CGPADDR (PDP IP) ─────────────
def test_cgpaddr_with_ip():
    assert parse_cgpaddr('+CGPADDR: 1,"10.64.0.42"') == "10.64.0.42"


def test_cgpaddr_unquoted():
    assert parse_cgpaddr("+CGPADDR: 1,10.64.0.42") == "10.64.0.42"


def test_cgpaddr_returns_none_when_no_pdp():
    assert parse_cgpaddr("+CGPADDR: 1") is None
    assert parse_cgpaddr("OK") is None
