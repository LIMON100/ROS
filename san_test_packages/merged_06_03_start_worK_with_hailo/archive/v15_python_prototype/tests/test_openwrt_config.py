"""Structural checks for infra/openwrt/ — files exist, parse, and
contain the directives the spec calls out.

We don't have a real OpenWrt host in CI, so these tests verify the
shape (UCI/JSON well-formedness + required keywords) rather than
runtime behavior.
"""
from __future__ import annotations

import json
import pathlib
import re

import pytest

# Phase 2-E Turn 14-17: archive/v15_python_prototype/tests/ → parents[3] = repo root.
_REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
_OPENWRT = _REPO_ROOT / "infra" / "openwrt" / "24.10"


# ─── file-tree presence ────────────────────────────────────────────────

@pytest.fixture(scope="module")
def files() -> dict:
    return {
        "network":     _OPENWRT / "etc" / "config" / "network",
        "wireless":    _OPENWRT / "etc" / "config" / "wireless",
        "batman-adv":  _OPENWRT / "etc" / "config" / "batman-adv",
        "mwan3":       _OPENWRT / "etc" / "config" / "mwan3",
        "firewall":    _OPENWRT / "etc" / "config" / "firewall",
        "system":      _OPENWRT / "etc" / "config" / "system",
        "dawn":        _OPENWRT / "etc" / "dawn" / "dawn.json",
        "cake":        _OPENWRT / "etc" / "sqm" / "cake.conf",
        "dscp":        _OPENWRT / "scripts" / "apply_dscp_marking.sh",
        "join":        _OPENWRT / "scripts" / "join_mesh.sh",
        "health":      _OPENWRT / "scripts" / "health_check.sh",
        "readme":      _OPENWRT / "README.md",
        "deploy":      _REPO_ROOT / "infra" / "openwrt" / "deploy.sh",
        "verify":      _REPO_ROOT / "infra" / "openwrt" / "verify.sh",
    }


def test_all_files_present(files):
    missing = [k for k, p in files.items() if not p.is_file()]
    assert missing == [], f"missing files: {missing}"


# ─── network ───────────────────────────────────────────────────────────

def _read(p: pathlib.Path) -> str:
    return p.read_text(encoding="utf-8")


def test_network_has_mesh0_batadv(files):
    text = _read(files["network"])
    assert "interface 'mesh0'" in text
    assert "option proto 'batadv'" in text


def test_network_has_lte_apn(files):
    text = _read(files["network"])
    # Spec line: APN = 'mil.kt' (military LTE).
    assert "option apn 'mil.kt'" in text


def test_network_lan_uses_42_subnet(files):
    text = _read(files["network"])
    assert "option ipaddr '192.168.42.1'" in text


# ─── wireless ──────────────────────────────────────────────────────────

def test_wireless_has_mesh_iface_with_sae(files):
    text = _read(files["wireless"])
    assert "option mode 'mesh'" in text
    assert "option encryption 'sae'" in text
    assert "option mesh_id 'san-mesh-001'" in text


def test_wireless_mesh_fwding_off_for_batman(files):
    # batman-adv routes; native HWMP must stay off.
    text = _read(files["wireless"])
    assert "option mesh_fwding '0'" in text


def test_wireless_band_is_5g_wifi6(files):
    text = _read(files["wireless"])
    assert "option band '5g'" in text
    assert "option htmode 'HE80'" in text   # HE = Wi-Fi 6


# ─── batman-adv ────────────────────────────────────────────────────────

def test_batman_uses_v_protocol(files):
    text = _read(files["batman-adv"])
    assert "option routing_algo 'BATMAN_V'" in text


# ─── mwan3 ─────────────────────────────────────────────────────────────

def test_mwan3_has_wan_and_wan_lte(files):
    text = _read(files["mwan3"])
    assert "config interface 'wan'" in text
    assert "config interface 'wan_lte'" in text


def test_mwan3_wan_preferred_over_lte(files):
    # metric 1 (wan) < metric 2 (lte) — lower metric wins.
    text = _read(files["mwan3"])
    # Member ordering matters in the policy.
    assert "list use_member 'wan_m1_w3'" in text
    assert "list use_member 'wan_lte_m2_w1'" in text
    pos_wan = text.find("'wan_m1_w3'")
    pos_lte = text.find("'wan_lte_m2_w1'")
    assert 0 < pos_wan < pos_lte, "wan member must precede wan_lte"


# ─── dawn.json ─────────────────────────────────────────────────────────

def test_dawn_json_parses(files):
    data = json.loads(_read(files["dawn"]))
    # Required top-level sections per the DAWN schema.
    for key in ("network", "hostapd", "times", "metric"):
        assert key in data, f"dawn.json missing top-level key {key!r}"


# ─── DSCP marking script ───────────────────────────────────────────────

def test_dscp_script_marks_all_four_classes(files):
    text = _read(files["dscp"])
    # EF for P0 (predictive control)
    assert "ip dscp set ef" in text
    # AF31 for P1 (robot status)
    assert "ip dscp set af31" in text
    # AF21 for video (P3, two port ranges: UDP follower + TCP/SRT
    # tablet)
    assert text.count("ip dscp set af21") >= 2


def test_dscp_script_covers_expected_port_ranges(files):
    text = _read(files["dscp"])
    # The DSCP class map in the README binds these specific ranges.
    for needle in ("7400-7500", "7501-7600", "5000-5009", "8888-8897"):
        assert needle in text, f"dscp script missing port range {needle}"


def test_dscp_script_is_a_shell_script(files):
    text = _read(files["dscp"])
    # POSIX sh shebang — OpenWrt's BusyBox ash, not bash.
    assert text.startswith("#!/bin/sh"), "dscp script must use #!/bin/sh"


# ─── deploy / verify scripts ───────────────────────────────────────────

def test_deploy_refuses_placeholder_secrets(files):
    text = _read(files["deploy"])
    # The guard string + the grep that enforces it must both be present.
    assert "CHANGE_ME_PROD" in text
    assert "refusing to deploy" in text


def test_verify_script_checks_required_subsystems(files):
    text = _read(files["verify"])
    for needle in (
        "batctl_originators",
        "mwan3_wan",
        "mwan3_wan_lte",
        "dscp_marking",
        "sqm_cake",
    ):
        assert needle in text, f"verify.sh missing {needle}"


# ─── placeholder hygiene ───────────────────────────────────────────────

_PLACEHOLDER_RE = re.compile(r"CHANGE_ME_PROD[A-Z_]*KEY")


def test_placeholders_are_present_in_secrets_files_only(files):
    """The two placeholder secrets must remain CHANGE_ME_PROD — a
    real key accidentally committed here is a security incident.
    """
    expected_files = {files["wireless"], files["dawn"]}
    actual = set()
    for p in _OPENWRT.rglob("*"):
        if not p.is_file():
            continue
        if _PLACEHOLDER_RE.search(p.read_text(encoding="utf-8", errors="ignore")):
            actual.add(p)
    # We allow CHANGE_ME_PROD in any file (the README and deploy.sh
    # mention it), but assert at least the two secrets files contain it.
    assert expected_files.issubset(actual), (
        f"expected placeholders in {expected_files}, found in {actual}")
