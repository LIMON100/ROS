"""Tests for ota_update.sh (P2-10)."""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "ota_update.sh"


def _need_bash():
    if shutil.which("bash") is None:
        pytest.skip("bash not on PATH")


@pytest.fixture
def env_with_tmp():
    """Isolated state dir + writable MISSION_LOCK location."""
    with tempfile.TemporaryDirectory() as td:
        manifest_dir = Path(td) / "patrol"
        manifest_dir.mkdir()
        env = os.environ.copy()
        env["MANIFEST_DIR"] = str(manifest_dir)
        env["PARTITION_A"] = "/tmp/fake_a"
        env["PARTITION_B"] = "/tmp/fake_b"
        env["OTA_BASE"] = "http://localhost:9999"  # unreachable on purpose
        env["MISSION_LOCK"] = str(Path(td) / "mission.active")
        yield env, manifest_dir, Path(td)


def test_script_exists():
    assert SCRIPT.exists()


def test_help():
    _need_bash()
    r = subprocess.run(["bash", str(SCRIPT), "-h"],
                       capture_output=True, text=True, timeout=10)
    combined = (r.stdout + r.stderr).lower()
    assert "usage:" in combined


def test_install_requires_version():
    _need_bash()
    r = subprocess.run(["bash", str(SCRIPT)],
                       capture_output=True, text=True, timeout=10)
    assert r.returncode != 0
    combined = (r.stdout + r.stderr).lower()
    assert "required" in combined


def test_status_default_partition_a(env_with_tmp):
    _need_bash()
    env, _, _ = env_with_tmp
    r = subprocess.run(["bash", str(SCRIPT), "-s"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 0
    assert "Active partition: A" in r.stdout


def test_status_after_active_b(env_with_tmp):
    _need_bash()
    env, manifest_dir, _ = env_with_tmp
    (manifest_dir / "active").write_text("B")
    r = subprocess.run(["bash", str(SCRIPT), "-s"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert "Active partition: B" in r.stdout


def test_rollback_swaps_partition(env_with_tmp):
    _need_bash()
    env, manifest_dir, _ = env_with_tmp
    (manifest_dir / "active").write_text("A")
    r = subprocess.run(["bash", str(SCRIPT), "-r"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 0
    new_active = (manifest_dir / "active").read_text().strip()
    assert new_active == "B"


def test_install_blocked_during_mission(env_with_tmp):
    """If MISSION_LOCK exists, install fails with exit code 2."""
    _need_bash()
    env, _, tmpdir = env_with_tmp
    lock = Path(env["MISSION_LOCK"])
    lock.touch()
    r = subprocess.run(["bash", str(SCRIPT), "-v", "v1.0.0"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 2


def test_commit_clears_pending(env_with_tmp):
    _need_bash()
    env, manifest_dir, _ = env_with_tmp
    (manifest_dir / "pending_commit").touch()
    r = subprocess.run(["bash", str(SCRIPT), "-c"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 0
    assert not (manifest_dir / "pending_commit").exists()
    assert "committed" in r.stdout.lower()


def test_commit_promotes_active_pending(env_with_tmp):
    """Audit P6 fix: commit branch must rename active.pending -> active
    and manifest.pending.json -> manifest.json. Previously the rename
    step was missing -- A/B flip silently no-op'd."""
    _need_bash()
    env, manifest_dir, _ = env_with_tmp
    # Simulate an install that staged into partition B.
    (manifest_dir / "active").write_text("A")
    (manifest_dir / "active.pending").write_text("B")
    (manifest_dir / "manifest.pending.json").write_text('{"version":"v1.4.2"}')
    (manifest_dir / "pending_commit").touch()

    r = subprocess.run(["bash", str(SCRIPT), "-c"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 0
    assert (manifest_dir / "active").read_text().strip() == "B"
    assert not (manifest_dir / "active.pending").exists()
    assert (manifest_dir / "manifest.json").read_text() == '{"version":"v1.4.2"}'
    assert not (manifest_dir / "manifest.pending.json").exists()
    assert not (manifest_dir / "pending_commit").exists()


def test_rollback_clears_pending_state(env_with_tmp):
    """Audit P6 follow-up: rollback discards any in-flight pending state
    so the next commit can't accidentally promote it."""
    _need_bash()
    env, manifest_dir, _ = env_with_tmp
    (manifest_dir / "active").write_text("A")
    (manifest_dir / "active.pending").write_text("B")
    (manifest_dir / "manifest.pending.json").write_text("{}")
    (manifest_dir / "pending_commit").touch()

    r = subprocess.run(["bash", str(SCRIPT), "-r"],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode == 0
    assert (manifest_dir / "active").read_text().strip() == "B"  # swapped
    for ghost in ("active.pending", "manifest.pending.json", "pending_commit"):
        assert not (manifest_dir / ghost).exists()


def test_unix_executable_bit():
    if os.name == "nt":
        pytest.skip("Windows: no exec bit semantics")
    assert os.access(str(SCRIPT), os.X_OK)
