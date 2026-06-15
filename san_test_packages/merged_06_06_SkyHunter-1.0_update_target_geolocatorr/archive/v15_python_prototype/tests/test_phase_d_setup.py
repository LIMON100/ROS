"""Tests for Phase D integration setup."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
SIM = REPO_ROOT / "sim"
SCRIPTS = SIM / "scripts"


def _need_bash():
    if shutil.which("bash") is None:
        pytest.skip("bash not on PATH")


# ───── Directory + file existence ─────
def test_sim_directory_exists():
    assert SIM.exists()
    for sub in ("gazebo", "launch", "scripts"):
        assert (SIM / sub).exists(), f"missing sim/{sub}"


def test_readme_exists():
    assert (SIM / "README.md").exists()


def test_docker_compose_exists_and_parses():
    f = SIM / "docker-compose.gazebo.yml"
    assert f.exists()
    data = yaml.safe_load(f.read_text(encoding="utf-8"))
    assert "services" in data
    assert "gazebo_world" in data["services"]


def test_launch_file_exists():
    assert (SIM / "launch" / "multi_robot_5.launch.py").exists()


def test_failure_injection_script_exists():
    assert (SCRIPTS / "inject_failure.sh").exists()


def test_kpp_measurement_script_exists():
    assert (SCRIPTS / "measure_kpp_in_sim.py").exists()


# ───── inject_failure.sh ─────
def test_failure_inject_help():
    _need_bash()
    r = subprocess.run(
        ["bash", str(SCRIPTS / "inject_failure.sh")],
        capture_output=True, text=True, timeout=10)
    combined = (r.stdout + r.stderr).lower()
    assert "usage:" in combined


def test_failure_inject_unknown_type_fails():
    _need_bash()
    r = subprocess.run(
        ["bash", str(SCRIPTS / "inject_failure.sh"), "unknown_type"],
        capture_output=True, text=True, timeout=10)
    assert r.returncode != 0


# ───── measure_kpp ─────
def test_kpp_script_runs_in_stub_mode():
    """Stub mode produces a valid JSON report."""
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "kpp_test.json"
        subprocess.run(
            [sys.executable, str(SCRIPTS / "measure_kpp_in_sim.py"),
             "--world", "empty_field",
             "--robots", "5",
             "--duration", "5",
             "--output", str(out)],
            capture_output=True, text=True, timeout=60)
        # Report should exist even if some KPP fails (it's deterministic
        # in stub mode so this is mostly checking the wiring).
        assert out.exists()
        report = json.loads(out.read_text(encoding="utf-8"))
        assert "kpps" in report
        for kpp_id in ("KPP-1", "KPP-2", "KPP-3", "KPP-4", "KPP-5"):
            assert kpp_id in report["kpps"]
            entry = report["kpps"][kpp_id]
            assert "measured" in entry
            assert "threshold" in entry
            assert "passed" in entry
            assert "unit" in entry


# ───── 5 KPPs match SDD spec ─────
def test_5_kpps_match_sdd_spec():
    sys.path.insert(0, str(SCRIPTS))
    try:
        from measure_kpp_in_sim import KPPS
    finally:
        sys.path.pop(0)
    assert len(KPPS) == 5
    assert set(KPPS.keys()) == {"KPP-1", "KPP-2", "KPP-3", "KPP-4", "KPP-5"}
    assert KPPS["KPP-1"]["threshold"] == 2.0
    assert KPPS["KPP-2"]["threshold"] == 0.3
    assert abs(KPPS["KPP-3"]["threshold"] - 0.150) < 1e-9
    assert KPPS["KPP-4"]["threshold"] == 10.0
    assert KPPS["KPP-5"]["threshold"] == 0.95


# ───── POSIX exec bit ─────
def test_unix_executable_bits():
    if os.name == "nt":
        pytest.skip("Windows: no exec bit semantics")
    for name in ("inject_failure.sh", "measure_kpp_in_sim.py"):
        assert os.access(str(SCRIPTS / name), os.X_OK), \
            f"missing exec bit on {name}"
