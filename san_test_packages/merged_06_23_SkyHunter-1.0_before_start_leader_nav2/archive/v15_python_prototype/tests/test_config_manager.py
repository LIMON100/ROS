"""Tests for ConfigManager (P2-11)."""
from __future__ import annotations

import tempfile
import threading
from pathlib import Path

import pytest

from core.config_manager import (
    CONFIG_FILES,
    ConfigImmutableError,
    ConfigManager,
)


@pytest.fixture
def tmp_manager():
    with tempfile.TemporaryDirectory() as td:
        cfg_dir = Path(td) / "config"
        snap_dir = Path(td) / "snapshots"
        cfg_dir.mkdir()
        for name in CONFIG_FILES:
            (cfg_dir / name).write_text(f"# {name}\nkey: value\n")
        yield ConfigManager(config_dir=str(cfg_dir),
                            snapshot_dir=str(snap_dir))


def test_initial_no_mission(tmp_manager):
    assert tmp_manager.is_mission_active is False


def test_take_snapshot_creates_files(tmp_manager):
    snap = tmp_manager.take_snapshot("M_test_001")
    assert snap.mission_id == "M_test_001"
    assert tmp_manager.is_mission_active is True
    snap_path = tmp_manager.snapshot_dir / snap.snapshot_id
    assert snap_path.exists()
    for name in CONFIG_FILES:
        assert (snap_path / name).exists()


def test_snapshot_includes_hashes(tmp_manager):
    snap = tmp_manager.take_snapshot("M1")
    assert len(snap.config_files) == len(CONFIG_FILES)
    for hsh in snap.config_files.values():
        assert len(hsh) == 64  # sha256 hex


def test_double_snapshot_raises(tmp_manager):
    tmp_manager.take_snapshot("M1")
    with pytest.raises(ConfigImmutableError):
        tmp_manager.take_snapshot("M2")


def test_write_blocked_during_mission(tmp_manager):
    tmp_manager.take_snapshot("M1")
    with pytest.raises(ConfigImmutableError):
        tmp_manager.write_config("system.yaml", "new: content")


def test_write_allowed_after_end(tmp_manager):
    tmp_manager.take_snapshot("M1")
    tmp_manager.end_mission(success=True)
    tmp_manager.write_config("system.yaml", "updated: True\n")
    content = (tmp_manager.config_dir / "system.yaml").read_text()
    assert "updated" in content


def test_end_mission_returns_snapshot(tmp_manager):
    snap = tmp_manager.take_snapshot("M1")
    ended = tmp_manager.end_mission()
    assert ended is not None
    assert ended.snapshot_id == snap.snapshot_id


def test_end_without_active_returns_none(tmp_manager):
    assert tmp_manager.end_mission() is None


def test_list_snapshots(tmp_manager):
    tmp_manager.take_snapshot("M1")
    tmp_manager.end_mission()
    tmp_manager.take_snapshot("M2")
    tmp_manager.end_mission()
    snaps = tmp_manager.list_snapshots()
    assert len(snaps) == 2
    assert {s.mission_id for s in snaps} == {"M1", "M2"}


def test_rollback_restores_config(tmp_manager):
    snap = tmp_manager.take_snapshot("M1")
    tmp_manager.end_mission()
    tmp_manager.write_config("system.yaml", "MODIFIED")
    success = tmp_manager.rollback_to(snap.snapshot_id)
    assert success is True
    content = (tmp_manager.config_dir / "system.yaml").read_text()
    assert "MODIFIED" not in content
    assert "value" in content


def test_rollback_unknown_snapshot_returns_false(tmp_manager):
    assert tmp_manager.rollback_to("nonexistent_id") is False


def test_rollback_blocked_during_mission(tmp_manager):
    snap = tmp_manager.take_snapshot("M1")
    with pytest.raises(ConfigImmutableError):
        tmp_manager.rollback_to(snap.snapshot_id)


def test_thread_safety():
    """Concurrent take_snapshot from multiple threads — exactly one wins."""
    with tempfile.TemporaryDirectory() as td:
        cfg_dir = Path(td) / "config"
        cfg_dir.mkdir()
        for name in CONFIG_FILES:
            (cfg_dir / name).write_text("x")
        cm = ConfigManager(config_dir=str(cfg_dir),
                           snapshot_dir=str(Path(td) / "snaps"))

        results: list = []

        def worker(mission_id: str) -> None:
            try:
                cm.take_snapshot(mission_id)
                results.append(("ok", mission_id))
            except ConfigImmutableError:
                results.append(("blocked", mission_id))

        threads = [threading.Thread(target=worker, args=(f"M{i}",))
                   for i in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        ok = [r for r in results if r[0] == "ok"]
        blocked = [r for r in results if r[0] == "blocked"]
        assert len(ok) == 1
        assert len(blocked) == 4
