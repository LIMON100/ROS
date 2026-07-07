"""Tests for comm.mesh_monitor — OpenWrt mesh + WAN failover poll."""
import pytest

from comm.mesh_monitor import (
    DEFAULT_POLL_PERIOD_S,
    MeshMonitor,
    parse_batctl_originators,
    parse_mwan3_status,
)
from core.messages import MeshStatus

# ─── batctl parser ─────────────────────────────────────────────────────

def test_batctl_empty_output_zero_peers():
    assert parse_batctl_originators("") == 0


def test_batctl_header_only_zero_peers():
    # `batctl o -H` removes the header but a `batctl o` (no -H) keeps
    # the column row — we should still ignore it.
    text = (
        "Originator        last-seen    (#/255)   Nexthop           [outgoingIF]\n"
    )
    assert parse_batctl_originators(text) == 0


def test_batctl_counts_one_peer_per_line():
    text = (
        "de:ad:be:ef:00:01    1.234s   (200) ae:bb:cc:dd:ee:ff [   mesh0]\n"
        "de:ad:be:ef:00:02    2.345s   (190) ae:bb:cc:dd:ee:fe [   mesh0]\n"
        "de:ad:be:ef:00:03    0.456s   (210) ae:bb:cc:dd:ee:fd [   mesh0]\n"
    )
    assert parse_batctl_originators(text) == 3


def test_batctl_skips_blank_lines():
    text = "\nde:ad:be:ef:00:01    1.234s   (200) ae:bb:cc:dd:ee:ff [   mesh0]\n\n"
    assert parse_batctl_originators(text) == 1


def test_batctl_skips_bracketed_footnotes():
    # Some versions print a `[ Translation table ]` footer.
    text = (
        "de:ad:be:ef:00:01    1.234s   (200) ae:bb:cc:dd:ee:ff [   mesh0]\n"
        "[ Translation table ]\n"
    )
    assert parse_batctl_originators(text) == 1


# ─── mwan3 parser ──────────────────────────────────────────────────────

def test_mwan3_primary_online_lte_offline():
    text = (
        "interface wan is online and tracking is active\n"
        "interface wan_lte is offline and tracking is active\n"
    )
    state = parse_mwan3_status(text)
    assert state == {"wan": "online", "wan_lte": "offline"}


def test_mwan3_both_offline():
    text = (
        "interface wan is offline and tracking is active\n"
        "interface wan_lte is offline and tracking is active\n"
    )
    state = parse_mwan3_status(text)
    assert state["wan"] == "offline"
    assert state["wan_lte"] == "offline"


def test_mwan3_ignores_unrelated_lines():
    text = (
        "Current ruleset:\n"
        " default_rule (out wan_then_lte)\n"
        "interface wan is online and tracking is active\n"
        "Policy wan_then_lte:\n"
    )
    state = parse_mwan3_status(text)
    assert state == {"wan": "online"}


def test_mwan3_handles_case_insensitive_online():
    # Some mwan3 versions print "Online" capitalised.
    text = "interface wan is Online and tracking is active\n"
    state = parse_mwan3_status(text)
    assert state["wan"] == "online"


# ─── MeshMonitor construction ──────────────────────────────────────────

def _make_runner(batctl_out: str, mwan3_out: str):
    def run(cmd: str) -> str:
        if cmd.startswith("batctl"):
            return batctl_out
        if cmd.startswith("mwan3"):
            return mwan3_out
        raise AssertionError(f"unexpected cmd: {cmd!r}")
    return run


def test_constructor_rejects_zero_period():
    with pytest.raises(ValueError):
        MeshMonitor(cmd_runner=lambda c: "", poll_period_s=0)


def test_constructor_rejects_empty_mesh_id():
    with pytest.raises(ValueError):
        MeshMonitor(cmd_runner=lambda c: "", mesh_id="")


def test_default_period_is_5s():
    m = MeshMonitor(cmd_runner=lambda c: "")
    assert m.poll_period_s == DEFAULT_POLL_PERIOD_S == 5.0


# ─── MeshMonitor.poll ──────────────────────────────────────────────────

def test_poll_builds_status_from_healthy_mesh():
    batctl = (
        "de:ad:be:ef:00:01    1s   (200) ae:bb:cc:dd:ee:ff [   mesh0]\n"
        "de:ad:be:ef:00:02    2s   (190) ae:bb:cc:dd:ee:fe [   mesh0]\n"
    )
    mwan3 = (
        "interface wan is online and tracking is active\n"
        "interface wan_lte is online and tracking is active\n"
    )
    m = MeshMonitor(cmd_runner=_make_runner(batctl, mwan3))
    msg = m.poll(now_ms=1_700_000_000_000)
    assert isinstance(msg, MeshStatus)
    assert msg.peer_count == 2
    assert msg.wan_primary_alive is True
    # Both online → not failover_active (primary is preferred).
    assert msg.wan_failover_active is False
    assert msg.mesh_id == "san-mesh-001"
    assert msg.timestamp_ms == 1_700_000_000_000


def test_poll_detects_wan_failover():
    batctl = "de:ad:be:ef:00:01 1s (200) ae:bb:cc:dd:ee:ff [mesh0]\n"
    mwan3 = (
        "interface wan is offline and tracking is active\n"
        "interface wan_lte is online and tracking is active\n"
    )
    m = MeshMonitor(cmd_runner=_make_runner(batctl, mwan3))
    msg = m.poll()
    assert msg.wan_primary_alive is False
    assert msg.wan_failover_active is True


def test_poll_detects_total_wan_outage():
    batctl = "de:ad:be:ef:00:01 1s (200) ae:bb:cc:dd:ee:ff [mesh0]\n"
    mwan3 = (
        "interface wan is offline and tracking is active\n"
        "interface wan_lte is offline and tracking is active\n"
    )
    m = MeshMonitor(cmd_runner=_make_runner(batctl, mwan3))
    msg = m.poll()
    assert msg.wan_primary_alive is False
    # Failover only true when LTE is online; both offline = false.
    assert msg.wan_failover_active is False


def test_poll_returns_none_on_runner_failure():
    def failing(cmd):
        raise RuntimeError("ssh broke")
    m = MeshMonitor(cmd_runner=failing)
    assert m.poll() is None
    assert m.stats["parse_failures"] == 1
    assert m.stats["polls"] == 1


def test_poll_custom_mesh_id_round_trips():
    batctl = "de:ad:be:ef:00:01 1s (200) ae:bb:cc:dd:ee:ff [mesh0]\n"
    mwan3 = "interface wan is online and tracking is active\n"
    m = MeshMonitor(
        cmd_runner=_make_runner(batctl, mwan3), mesh_id="prod-mesh-42")
    msg = m.poll()
    assert msg.mesh_id == "prod-mesh-42"


def test_poll_message_validates():
    batctl = "de:ad:be:ef:00:01 1s (200) ae:bb:cc:dd:ee:ff [mesh0]\n"
    mwan3 = (
        "interface wan is offline and tracking is active\n"
        "interface wan_lte is online and tracking is active\n"
    )
    m = MeshMonitor(cmd_runner=_make_runner(batctl, mwan3))
    msg = m.poll()
    msg.validate()  # roundtrips the failover-inconsistency check


# ─── MeshStatus validate ──────────────────────────────────────────────

def test_mesh_status_defaults_validate():
    MeshStatus().validate()


def test_mesh_status_rejects_failover_active_with_primary_alive():
    # Inconsistent: if primary is alive we wouldn't be in failover.
    with pytest.raises(ValueError):
        MeshStatus(
            wan_primary_alive=True,
            wan_failover_active=True,
        ).validate()


def test_mesh_status_rejects_negative_peer_count():
    with pytest.raises(ValueError):
        MeshStatus(peer_count=-1).validate()


def test_mesh_status_rejects_negative_timestamp():
    with pytest.raises(ValueError):
        MeshStatus(timestamp_ms=-1).validate()
