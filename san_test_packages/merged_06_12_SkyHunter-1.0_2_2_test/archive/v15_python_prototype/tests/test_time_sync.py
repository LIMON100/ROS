"""Tests for PTP time synchronization (P1-15, SDD Rev.A.6 §5.8)."""
from __future__ import annotations

from core.time_sync import (
    PtpController,
    TimeSyncSource,
    quorum_consensus_offset,
)


def fake_runner_success(cmd, timeout=5.0):
    if "ptp4l" in cmd[0]:
        return 0, "", ""
    if "pmc" in cmd[0]:
        return 0, ("ME_TRANSPORT_SPECIFIC 0x0\n"
                   "  master_offset 250000\n"          # 250 us = 0.25 ms
                   "  ingress_time 12345\n"), ""
    return -1, "", "unknown"


def fake_runner_fail(cmd, timeout=5.0):
    return -1, "", "command not found"


# ─── Initial state + lifecycle ───
def test_initial_status_is_none():
    p = PtpController(runner=fake_runner_fail)
    s = p.get_status()
    assert s.source == TimeSyncSource.NONE
    assert s.offset_ms == 999.0


def test_master_mode_starts_ptp4l_without_slave_flag():
    cmd_log: list = []

    def runner(cmd, timeout=5.0):
        cmd_log.append(cmd)
        return 0, "", ""

    p = PtpController(is_master=True, runner=runner)
    p.start()
    p.stop()
    ptp_cmds = [c for c in cmd_log if c and c[0] == "ptp4l"]
    assert ptp_cmds
    assert "-s" not in ptp_cmds[0]


def test_slave_mode_includes_slave_flag():
    cmd_log: list = []

    def runner(cmd, timeout=5.0):
        cmd_log.append(cmd)
        return 0, "", ""

    p = PtpController(is_master=False, runner=runner)
    p.start()
    p.stop()
    ptp_cmds = [c for c in cmd_log if c and c[0] == "ptp4l"]
    assert ptp_cmds
    assert "-s" in ptp_cmds[0]


# ─── Mission ready predicate ───
def test_check_mission_ready_blocks_on_high_offset():
    p = PtpController(runner=fake_runner_success)
    p._status.source = TimeSyncSource.PTP_SLAVE
    p._status.offset_ms = 10.0
    ok, msg = p.check_mission_ready()
    assert ok is False
    assert "exceeds" in msg or "5" in msg


def test_check_mission_ready_passes_with_low_offset():
    p = PtpController(runner=fake_runner_success)
    p._status.source = TimeSyncSource.PTP_SLAVE
    p._status.offset_ms = 0.5
    ok, _msg = p.check_mission_ready()
    assert ok is True


def test_check_mission_ready_blocks_on_no_ptp():
    p = PtpController(runner=fake_runner_fail)
    ok, msg = p.check_mission_ready()
    assert ok is False
    assert "PTP" in msg or "active" in msg


# ─── pmc parser ───
def test_parse_pmc_offset_us_to_ms():
    text = "  master_offset 1500000\n"        # 1.5 ms
    offset = PtpController._parse_pmc_offset(text)
    assert abs(offset - 1.5) < 0.01


def test_parse_pmc_negative_offset():
    text = "  master_offset -2000000\n"
    offset = PtpController._parse_pmc_offset(text)
    assert offset == 2.0


def test_parse_pmc_unparseable_returns_sentinel():
    assert PtpController._parse_pmc_offset("garbage\n") == 999.0


# ─── quorum fallback ───
def test_quorum_consensus_median():
    offsets = [0.5, 0.3, 0.4, 0.6, 0.5]
    assert abs(quorum_consensus_offset(offsets) - 0.5) < 0.01


def test_quorum_consensus_empty():
    assert quorum_consensus_offset([]) == 999.0


def test_quorum_consensus_even_count():
    offsets = [1.0, 2.0, 3.0, 4.0]            # median = 2.5
    assert abs(quorum_consensus_offset(offsets) - 2.5) < 0.01
