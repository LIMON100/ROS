"""Configuration management — mission-level snapshot + rollback (SDD §10.6.3, P2-11).

Per-mission immutable snapshot of:
- system.yaml, tactical_missions.yaml, geofence.yaml
- model versions (.rknn paths + sha256)
- NTRIP credentials reference (encrypted, never plain text)

Mission active: writes blocked (raises ConfigImmutableError).
Mission end: snapshot archived + linkable in audit log (P1-16).
"""
from __future__ import annotations

import hashlib
import json
import shutil
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

CONFIG_FILES = ["system.yaml", "tactical_missions.yaml", "geofence.yaml"]


class ConfigImmutableError(Exception):
    """Raised on attempt to write/rollback config during active mission."""


@dataclass
class ConfigSnapshot:
    snapshot_id: str
    mission_id: str
    created_ts: float
    config_files: Dict[str, str] = field(default_factory=dict)  # name → sha256
    model_versions: Dict[str, str] = field(default_factory=dict)
    ntrip_ref: Optional[str] = None


class ConfigManager:
    """Mission-aware config snapshot manager."""

    DEFAULT_CONFIG_DIR = "config"
    DEFAULT_SNAPSHOT_DIR = "/var/lib/patrol/config_snapshots"

    def __init__(self,
                 config_dir: str = DEFAULT_CONFIG_DIR,
                 snapshot_dir: str = DEFAULT_SNAPSHOT_DIR) -> None:
        self.config_dir = Path(config_dir)
        self.snapshot_dir = Path(snapshot_dir)
        self.snapshot_dir.mkdir(parents=True, exist_ok=True)
        self._mission_active: bool = False
        self._active_mission_id: Optional[str] = None
        self._active_snapshot: Optional[ConfigSnapshot] = None
        self._lock = threading.Lock()

    @staticmethod
    def _hash_file(path: Path) -> str:
        h = hashlib.sha256()
        with path.open("rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                h.update(chunk)
        return h.hexdigest()

    def take_snapshot(self, mission_id: str) -> ConfigSnapshot:
        """Called at mission start. Creates immutable snapshot."""
        with self._lock:
            if self._mission_active:
                raise ConfigImmutableError(
                    f"mission {self._active_mission_id} already active")
            snap_id = f"{mission_id}_{int(time.time() * 1000)}"
            snap_dir = self.snapshot_dir / snap_id
            snap_dir.mkdir(parents=True, exist_ok=True)

            file_hashes: Dict[str, str] = {}
            for name in CONFIG_FILES:
                src = self.config_dir / name
                if src.exists():
                    dst = snap_dir / name
                    shutil.copy2(src, dst)
                    file_hashes[name] = self._hash_file(dst)

            model_versions: Dict[str, str] = {}
            models_dir = Path("models")
            if models_dir.exists():
                for rknn in models_dir.glob("*.rknn"):
                    model_versions[rknn.name] = self._hash_file(rknn)[:16]

            snapshot = ConfigSnapshot(
                snapshot_id=snap_id,
                mission_id=mission_id,
                created_ts=time.time(),
                config_files=file_hashes,
                model_versions=model_versions,
                ntrip_ref=None,
            )
            with (snap_dir / "snapshot.json").open("w") as f:
                json.dump(asdict(snapshot), f, indent=2)

            self._mission_active = True
            self._active_mission_id = mission_id
            self._active_snapshot = snapshot
            return snapshot

    def end_mission(self, success: bool = True) -> Optional[ConfigSnapshot]:
        """Called at mission end. Returns active snapshot for audit linkage."""
        with self._lock:
            if not self._mission_active:
                return None
            snap = self._active_snapshot
            self._mission_active = False
            self._active_mission_id = None
            self._active_snapshot = None
            return snap

    def write_config(self, name: str, content: str) -> None:
        """Writes blocked during active mission."""
        with self._lock:
            if self._mission_active:
                raise ConfigImmutableError(
                    f"config write blocked during mission "
                    f"{self._active_mission_id}")
            (self.config_dir / name).write_text(content)

    def list_snapshots(self) -> List[ConfigSnapshot]:
        result: List[ConfigSnapshot] = []
        for snap_dir in sorted(self.snapshot_dir.iterdir()):
            json_file = snap_dir / "snapshot.json"
            if json_file.exists():
                with json_file.open() as f:
                    data = json.load(f)
                result.append(ConfigSnapshot(**data))
        return result

    def rollback_to(self, snapshot_id: str) -> bool:
        """Restore config from snapshot. Blocked during active mission."""
        with self._lock:
            if self._mission_active:
                raise ConfigImmutableError(
                    "rollback blocked during active mission")
            snap_dir = self.snapshot_dir / snapshot_id
            if not snap_dir.exists():
                return False
            for name in CONFIG_FILES:
                src = snap_dir / name
                if src.exists():
                    dst = self.config_dir / name
                    shutil.copy2(src, dst)
            return True

    @property
    def is_mission_active(self) -> bool:
        with self._lock:
            return self._mission_active
