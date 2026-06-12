"""Tests for the Hub UGV dual-SBC deployment (PHASE 5).

Verifies the entry-point scripts import + smoke-run cleanly inside the
test interpreter, and that docker-compose.hub.yml has the structure the
deployment relies on (image tags, host network, /dev/nvme0n1, NET_ADMIN).
"""
from __future__ import annotations

import importlib.util
import pathlib
from typing import Any, Dict

import pytest
import yaml

_REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
_COMPOSE_PATH = _REPO_ROOT / "sim" / "docker-compose.hub.yml"

# The entry-point scripts depend on `mapping.slam_aggregator` (PR #35)
# and `streaming.gstreamer_relay` (PR #36). When this PHASE 5 branch is
# checked out before those merge to main, the import-time tests cannot
# run — skip them with a clear reason rather than reporting a false
# failure. Once #35/#36 are merged, this branch will auto-pick them up
# and the skips disappear.
_SLAM_AGG_AVAILABLE = importlib.util.find_spec(
    "mapping.slam_aggregator") is not None
_GSTREAMER_RELAY_AVAILABLE = importlib.util.find_spec(
    "streaming.gstreamer_relay") is not None
_ENTRYPOINT_DEPS_AVAILABLE = (
    _SLAM_AGG_AVAILABLE and _GSTREAMER_RELAY_AVAILABLE)
_ENTRYPOINT_SKIP_REASON = (
    "entry-point dependencies (PR #35 / #36) not yet on main")


def _load_script_module(rel_path: str, mod_name: str):
    """Load scripts/<rel_path> as a module without touching sys.path globally."""
    full = _REPO_ROOT / "scripts" / rel_path
    spec = importlib.util.spec_from_file_location(mod_name, full)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ─── compose file structure ────────────────────────────────────────────

@pytest.fixture(scope="module")
def compose() -> Dict[str, Any]:
    text = _COMPOSE_PATH.read_text(encoding="utf-8")
    return yaml.safe_load(text)


def test_compose_file_exists():
    assert _COMPOSE_PATH.is_file()


def test_compose_has_both_sbc_services(compose):
    services = compose.get("services", {})
    assert set(services) >= {"hub-sbc1", "hub-sbc2"}


def test_compose_sbc1_image_tag(compose):
    assert compose["services"]["hub-sbc1"]["image"] == "san/hub-slam:1.1"


def test_compose_sbc2_image_tag(compose):
    assert compose["services"]["hub-sbc2"]["image"] == "san/hub-comm:1.1"


def test_compose_uses_host_network(compose):
    # DDS multicast requires host networking on both SBCs.
    assert compose["services"]["hub-sbc1"]["network_mode"] == "host"
    assert compose["services"]["hub-sbc2"]["network_mode"] == "host"


def test_compose_sbc1_passes_nvme_device(compose):
    devices = compose["services"]["hub-sbc1"].get("devices", [])
    assert "/dev/nvme0n1" in devices


def test_compose_sbc2_has_net_admin(compose):
    caps = compose["services"]["hub-sbc2"].get("cap_add", [])
    assert "NET_ADMIN" in caps


def test_compose_sets_ros_domain_id(compose):
    for svc in ("hub-sbc1", "hub-sbc2"):
        env = compose["services"][svc].get("environment", [])
        assert "ROS_DOMAIN_ID=42" in env


def test_compose_build_directives_present(compose):
    # `build:` directives let `docker compose build` produce the image
    # tags locally without a registry round-trip.
    s1 = compose["services"]["hub-sbc1"]
    s2 = compose["services"]["hub-sbc2"]
    assert s1["build"]["dockerfile"] == "sim/Dockerfile.hub-slam"
    assert s2["build"]["dockerfile"] == "sim/Dockerfile.hub-comm"


def test_dockerfile_paths_resolve():
    assert (_REPO_ROOT / "sim" / "Dockerfile.hub-slam").is_file()
    assert (_REPO_ROOT / "sim" / "Dockerfile.hub-comm").is_file()


# ─── entry-point modules ───────────────────────────────────────────────

@pytest.mark.skipif(not _SLAM_AGG_AVAILABLE, reason=_ENTRYPOINT_SKIP_REASON)
def test_run_hub_slam_module_imports():
    mod = _load_script_module("run_hub_slam.py", "_test_run_hub_slam")
    # Module-level imports include SlamAggregator + PERIOD_BY_MODE.
    assert hasattr(mod, "main")
    assert hasattr(mod, "SlamAggregator")


@pytest.mark.skipif(not _GSTREAMER_RELAY_AVAILABLE,
                     reason=_ENTRYPOINT_SKIP_REASON)
def test_run_hub_comm_module_imports():
    mod = _load_script_module("run_hub_comm.py", "_test_run_hub_comm")
    assert hasattr(mod, "main")
    assert hasattr(mod, "GStreamerRelay")


@pytest.mark.skipif(not _SLAM_AGG_AVAILABLE, reason=_ENTRYPOINT_SKIP_REASON)
def test_run_hub_slam_smoke_exits_zero(capsys):
    mod = _load_script_module("run_hub_slam.py", "_test_run_hub_slam_smoke")
    rc = mod.main(["--smoke"])
    assert rc == 0


@pytest.mark.skipif(not _GSTREAMER_RELAY_AVAILABLE,
                     reason=_ENTRYPOINT_SKIP_REASON)
def test_run_hub_comm_smoke_exits_zero(capsys):
    mod = _load_script_module("run_hub_comm.py", "_test_run_hub_comm_smoke")
    rc = mod.main(["--smoke"])
    assert rc == 0
