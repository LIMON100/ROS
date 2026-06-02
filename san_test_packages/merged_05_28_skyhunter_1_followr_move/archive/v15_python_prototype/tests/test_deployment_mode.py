"""
Tests for SAN v1.3 §11 deployment_mode + 8-robot ID mapping (PHASE 0).

Covers:
  • DeploymentMode enum parse + invalid-value rejection
  • 5 mode overlays load cleanly via Config.load(); resolved mode lands
    in `cfg.deployment_mode` and propagates through to OperationState
  • Robot-ID constants are stable and deputy-chain validation rejects
    leader inclusion / out-of-range / empty fallback
  • DEVELOPER_AUTH_TOKEN gate: development without token raises, with
    token passes; other modes are never gated
  • Mode transition policy: production↔demo allowed, production→lab_test
    denied, any→development conditional, development→any denied
  • Missing overlay yaml is a hard FileNotFoundError (no silent fallback)
"""
from __future__ import annotations

import os
import textwrap
from pathlib import Path

import pytest

from core import (
    DEFAULT_DEPUTY_CHAIN,
    DEVELOPER_AUTH_ENV,
    FOLLOWER_ROBOT_IDS,
    HUB_ROBOT_ID,
    LEADER_ROBOT_ID,
    MAX_ROBOTS,
    Config,
    DeploymentMode,
    DeveloperAuthError,
    OperationState,
    is_mode_transition_allowed,
    normalize_deputy_chain,
    validate_developer_auth,
)
from core.deployment import build_operation_state
from core.ipc import consume, make_topic_queues, publish


# ────────────────────────────────────────────────────────────────────
# 8-robot ID mapping (SAN v1.3 §11)
# ────────────────────────────────────────────────────────────────────
def test_robot_id_constants_match_san_v13_spec():
    assert MAX_ROBOTS == 8
    assert LEADER_ROBOT_ID == 1
    assert HUB_ROBOT_ID == 2
    assert FOLLOWER_ROBOT_IDS == (3, 4, 5, 6, 7, 8)
    assert DEFAULT_DEPUTY_CHAIN == (2, 3, 4, 5, 6, 7, 8)
    # Hub is the first deputy by construction.
    assert DEFAULT_DEPUTY_CHAIN[0] == HUB_ROBOT_ID
    # Leader never appears in the deputy chain.
    assert LEADER_ROBOT_ID not in DEFAULT_DEPUTY_CHAIN


def test_normalize_deputy_chain_strips_duplicates():
    assert normalize_deputy_chain([2, 3, 2, 4]) == (2, 3, 4)


def test_normalize_deputy_chain_falls_back_on_empty():
    assert normalize_deputy_chain(None) == DEFAULT_DEPUTY_CHAIN
    assert normalize_deputy_chain([]) == DEFAULT_DEPUTY_CHAIN


def test_normalize_deputy_chain_rejects_leader():
    with pytest.raises(ValueError, match="LEADER_ROBOT_ID"):
        normalize_deputy_chain([1, 2, 3])


@pytest.mark.parametrize("bad", [0, -1, 9, 99])
def test_normalize_deputy_chain_rejects_out_of_range(bad):
    with pytest.raises(ValueError, match="out of range"):
        normalize_deputy_chain([bad])


# ────────────────────────────────────────────────────────────────────
# DeploymentMode enum
# ────────────────────────────────────────────────────────────────────
@pytest.mark.parametrize("mode_str,enum", [
    ("production",  DeploymentMode.PRODUCTION),
    ("demo",        DeploymentMode.DEMO),
    ("lab_test",    DeploymentMode.LAB_TEST),
    ("bench",       DeploymentMode.BENCH),
    ("development", DeploymentMode.DEVELOPMENT),
    ("PRODUCTION",  DeploymentMode.PRODUCTION),    # case-insensitive
    (" demo ",      DeploymentMode.DEMO),          # whitespace tolerated
])
def test_parse_accepts_all_five_tiers(mode_str, enum):
    assert DeploymentMode.parse(mode_str) is enum


def test_parse_rejects_unknown_mode():
    with pytest.raises(ValueError, match="unknown deployment_mode"):
        DeploymentMode.parse("staging")


def test_parse_rejects_none():
    with pytest.raises(ValueError):
        DeploymentMode.parse(None)


def test_is_live_robot_classifies_correctly():
    assert DeploymentMode.PRODUCTION.is_live_robot
    assert DeploymentMode.DEMO.is_live_robot
    assert DeploymentMode.LAB_TEST.is_live_robot
    assert not DeploymentMode.BENCH.is_live_robot
    assert not DeploymentMode.DEVELOPMENT.is_live_robot


def test_allows_stubs_classifies_correctly():
    assert DeploymentMode.BENCH.allows_stubs
    assert DeploymentMode.DEVELOPMENT.allows_stubs
    assert not DeploymentMode.PRODUCTION.allows_stubs
    assert not DeploymentMode.DEMO.allows_stubs
    assert not DeploymentMode.LAB_TEST.allows_stubs


# ────────────────────────────────────────────────────────────────────
# Mode transition policy
# ────────────────────────────────────────────────────────────────────
def test_identity_transition_always_allowed():
    for m in DeploymentMode:
        assert is_mode_transition_allowed(m, m), m


def test_production_to_demo_allowed():
    assert is_mode_transition_allowed(
        DeploymentMode.PRODUCTION, DeploymentMode.DEMO)
    assert is_mode_transition_allowed(
        DeploymentMode.DEMO, DeploymentMode.PRODUCTION)


def test_production_to_lab_test_denied():
    """Live robots must not silently drop into reduced-safety mode."""
    assert not is_mode_transition_allowed(
        DeploymentMode.PRODUCTION, DeploymentMode.LAB_TEST)


def test_any_to_development_allowed_at_policy_layer():
    """The token gate is the security boundary, not the policy table."""
    for src in DeploymentMode:
        if src is DeploymentMode.DEVELOPMENT:
            continue
        assert is_mode_transition_allowed(src, DeploymentMode.DEVELOPMENT)


def test_development_to_anything_denied():
    """A dev session must end with a reboot, not a runtime promotion."""
    for dst in DeploymentMode:
        if dst is DeploymentMode.DEVELOPMENT:
            continue
        assert not is_mode_transition_allowed(
            DeploymentMode.DEVELOPMENT, dst), dst


def test_bench_lab_test_bidirectional():
    assert is_mode_transition_allowed(
        DeploymentMode.BENCH, DeploymentMode.LAB_TEST)
    assert is_mode_transition_allowed(
        DeploymentMode.LAB_TEST, DeploymentMode.BENCH)


# ────────────────────────────────────────────────────────────────────
# DEVELOPER_AUTH_TOKEN gate
# ────────────────────────────────────────────────────────────────────
def test_validate_developer_auth_passthrough_for_non_dev():
    for m in DeploymentMode:
        if m is DeploymentMode.DEVELOPMENT:
            continue
        # Empty env dict → still fine, the gate is dev-only.
        validate_developer_auth(m, env={})


def test_validate_developer_auth_denies_without_token():
    with pytest.raises(DeveloperAuthError, match="DEVELOPER_AUTH_TOKEN"):
        validate_developer_auth(DeploymentMode.DEVELOPMENT, env={})


def test_validate_developer_auth_denies_empty_token():
    with pytest.raises(DeveloperAuthError):
        validate_developer_auth(
            DeploymentMode.DEVELOPMENT,
            env={DEVELOPER_AUTH_ENV: "   "})


def test_validate_developer_auth_accepts_token():
    # Must not raise.
    validate_developer_auth(
        DeploymentMode.DEVELOPMENT,
        env={DEVELOPER_AUTH_ENV: "dev-abc123"})


# ────────────────────────────────────────────────────────────────────
# Config.load() resolves all 5 modes and merges overlays
# ────────────────────────────────────────────────────────────────────
# Phase 2-E Turn 14-17: archive/v15_python_prototype/tests/ → parents[3] = repo root.
REPO_ROOT = Path(__file__).resolve().parents[3]
BASE_CFG = REPO_ROOT / "config" / "system.yaml"


@pytest.mark.parametrize("mode", [
    "production", "demo", "lab_test", "bench", "development",
])
def test_all_five_overlays_load(monkeypatch, mode):
    """Every overlay yaml exists and merges into a Config without error."""
    # Strip any developer token + env overrides from the parent shell so
    # the test is deterministic.
    for var in (DEVELOPER_AUTH_ENV,):
        monkeypatch.delenv(var, raising=False)
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)

    cfg = Config.load(str(BASE_CFG), deployment_mode=mode)
    assert cfg.deployment_mode == mode
    # Base values still resolved (overlay merged ON TOP, not REPLACED).
    assert cfg.get("system", "cpu_affinity") is not None
    assert cfg.get("go2", "interface") is not None


def test_overlay_deltas_actually_apply(monkeypatch):
    """lab_test overlay loosens RTK quality + tightens comm-loss timeout."""
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    base = Config.load(str(BASE_CFG), deployment_mode="production")
    lab = Config.load(str(BASE_CFG), deployment_mode="lab_test")
    # Base = strict (1.5s, σ ≤ 10 cm, 30s comm-loss); lab = relaxed for indoor.
    assert base.get("localization", "rtk_max_age_s") == 1.5
    assert lab.get("localization", "rtk_max_age_s") == 5.0
    assert lab.get("localization", "rtk_fixed_sigma_max") == 0.30
    assert lab.get("safety", "comm_loss_timeout_s") == 10
    # Sanity: base value is preserved for keys the overlay does NOT touch.
    assert lab.get("go2", "lidar_topic") == base.get("go2", "lidar_topic")


def test_missing_overlay_is_hard_error(tmp_path, monkeypatch):
    """A typo'd mode name must NOT silently fall back to production."""
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    base = tmp_path / "params.yaml"
    base.write_text("system:\n  deployment_mode: production\n")
    # No params.staging.yaml exists in tmp_path.
    with pytest.raises(FileNotFoundError, match="staging"):
        Config.load(str(base), deployment_mode="staging")


def test_cli_overrides_yaml_deployment_mode(tmp_path, monkeypatch):
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    base = tmp_path / "params.yaml"
    base.write_text(textwrap.dedent("""\
        system:
          deployment_mode: production
    """))
    # No overlay needed because we resolve to production from CLI.
    cfg = Config.load(str(base), deployment_mode="production")
    assert cfg.deployment_mode == "production"


def test_env_var_resolves_mode(tmp_path, monkeypatch):
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    base = tmp_path / "params.yaml"
    overlay = tmp_path / "params.demo.yaml"
    base.write_text("system:\n  deployment_mode: production\nfoo: bar\n")
    overlay.write_text("system:\n  deployment_mode: demo\nfoo: baz\n")
    monkeypatch.setenv("PATROL__SYSTEM__DEPLOYMENT_MODE", "demo")
    cfg = Config.load(str(base))
    assert cfg.deployment_mode == "demo"
    assert cfg.get("foo") == "baz"


# ────────────────────────────────────────────────────────────────────
# OperationState heartbeat
# ────────────────────────────────────────────────────────────────────
def test_build_operation_state_carries_deployment_mode(monkeypatch):
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    cfg = Config.load(str(BASE_CFG), deployment_mode="bench")
    msg = build_operation_state(cfg, n_alive_followers=3)
    assert isinstance(msg, OperationState)
    assert msg.deployment_mode == "bench"
    assert msg.leader_robot_id == LEADER_ROBOT_ID
    assert msg.hub_robot_id == HUB_ROBOT_ID
    assert msg.deputy_chain == DEFAULT_DEPUTY_CHAIN
    assert msg.n_alive_followers == 3


def test_operation_state_flows_through_queue(monkeypatch):
    """End-to-end: publish OperationState on the bus, consume the same."""
    for k in list(os.environ):
        if k.startswith("PATROL__"):
            monkeypatch.delenv(k, raising=False)
    queues = make_topic_queues(maxsize=4)
    try:
        cfg = Config.load(str(BASE_CFG), deployment_mode="lab_test")
        msg = build_operation_state(cfg)
        assert publish(queues.operation_state, msg) is True
        rx = consume(queues.operation_state, timeout=1.0)
        assert rx is not None
        assert rx.deployment_mode == "lab_test"
        assert rx.deputy_chain == DEFAULT_DEPUTY_CHAIN
    finally:
        # Drain + explicitly close so the mp.Queue feeder thread doesn't
        # outlive the test (conftest sweeps stragglers but cleanliness here
        # avoids spurious "queue still has items" warnings).
        try:
            queues.operation_state.close()
            queues.operation_state.join_thread()
        except Exception:        # nosec — best-effort cleanup
            pass
