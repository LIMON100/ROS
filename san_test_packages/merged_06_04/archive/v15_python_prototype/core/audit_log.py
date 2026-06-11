"""Audit log — hash-chained immutable JSONL (SDD Rev.A.6 §9.7).

Required for KRIT verification + military security audit + post-incident
analysis. Each entry stores the prev_hash and self_hash (sha256). Any
field tampering breaks the chain — verify_chain() locates the first
corrupt line.

Categories per §9.7.1:
  mission   — start / abort / complete + waypoints
  fsm       — phase transitions
  permission — PIN auth, dev_mode, geofence override
  safety    — E1..E4, RTH triggers
  comm      — failover events
  ai_decision — perception/anomaly detector outputs
  election  — Modified Raft term / vote / priority
  time_sync — PTP offset changes, GNSS PPS lock

File layout:
  /var/log/patrol/audit/<YYYYMMDD>_<idx>.jsonl
  Append-only, 10 MB per-file rotation, 7 GB total cap.
"""
from __future__ import annotations

import hashlib
import json
import logging
import os
import threading
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

GENESIS_HASH = "0" * 64

log = logging.getLogger(__name__)


class AuditLogger:
    """Append-only hash-chained logger. Thread-safe."""

    DEFAULT_LOG_DIR = "/var/log/patrol/audit"
    MAX_FILE_BYTES = 10 * 1024 * 1024            # 10 MB per file
    MAX_TOTAL_BYTES = 7 * 1024 * 1024 * 1024     # 7 GB across all files

    def __init__(self,
                 log_dir: str = DEFAULT_LOG_DIR,
                 robot_id: str = "robot-000",
                 on_drop: Optional[Callable[[Dict[str, Any], Exception],
                                            None]] = None):
        self.log_dir = Path(log_dir)
        self.robot_id = robot_id
        self.on_drop = on_drop
        self.dropped_count: int = 0
        self._lock = threading.Lock()
        self._prev_hash: str = GENESIS_HASH
        self._current_file: Optional[Path] = None
        self._current_size: int = 0
        self._init_chain()

    # ─── Internal init ───
    def _init_chain(self) -> None:
        """Resume the chain from the latest file.

        On JSONDecodeError (process crashed mid-write), the partial tail
        line is truncated off disk so the next append doesn't fall back
        to GENESIS_HASH in the middle of an otherwise-valid file. The
        last fully-parsed entry's self_hash becomes the new prev_hash.
        """
        try:
            self.log_dir.mkdir(parents=True, exist_ok=True)
        except OSError:
            return
        files = sorted(self.log_dir.glob("*.jsonl"))
        if not files:
            return
        last = files[-1]
        try:
            with last.open("rb") as f:
                f.seek(0, os.SEEK_END)
                size = f.tell()
                # Read up to 64 KiB so a multi-line partial-write at the
                # end (e.g. truncated huge params) is still salvageable.
                window = min(size, 65536)
                f.seek(size - window)
                tail = f.read().decode("utf-8", errors="ignore")
        except OSError:
            return

        # Find the last *fully parseable* line. If the trailing line is
        # malformed (no newline, truncated JSON), step back through the
        # buffer until we find a clean one.
        lines = tail.split("\n")
        # Drop the trailing empty produced by a clean newline.
        if lines and lines[-1] == "":
            lines.pop()
        last_good_hash = GENESIS_HASH
        last_good_idx: Optional[int] = None
        for idx in range(len(lines) - 1, -1, -1):
            raw = lines[idx]
            if not raw.strip():
                continue
            try:
                entry = json.loads(raw)
                last_good_hash = entry.get("self_hash", GENESIS_HASH)
                last_good_idx = idx
                break
            except json.JSONDecodeError:
                continue

        # If the *very last* line in the file was malformed, truncate
        # back to just past the last good line so we don't append after
        # garbage.
        if (last_good_idx is not None
                and last_good_idx != len(lines) - 1):
            # Bytes up to and including the last good line + its newline.
            good_text = "\n".join(lines[:last_good_idx + 1]) + "\n"
            keep_bytes = len(good_text.encode("utf-8"))
            absolute_keep = size - len(tail.encode("utf-8")) + keep_bytes
            try:
                with last.open("rb+") as f:
                    f.truncate(absolute_keep)
                size = absolute_keep
                log.warning(
                    "audit chain resume: truncated %d byte(s) of "
                    "partial tail from %s",
                    len(tail.encode("utf-8")) - keep_bytes, last.name)
            except OSError as e:
                log.error("audit chain resume: truncate failed: %s", e)

        self._prev_hash = last_good_hash
        self._current_file = last
        self._current_size = size

    # ─── Public API ───
    def log(self,
            category: str,
            event: str,
            actor: Optional[str] = None,
            params: Optional[Dict[str, Any]] = None,
            fsm_phase: Optional[str] = None,
            mission_id: Optional[str] = None) -> str:
        """Append an audit entry. Returns the new self_hash."""
        entry: Dict[str, Any] = {
            "ts_iso": datetime.now(timezone.utc).isoformat(
                timespec="milliseconds"),
            "robot_id": self.robot_id,
            "category": category,
            "event": event,
            "actor": actor,
            "params": params or {},
            "fsm_phase": fsm_phase,
            "mission_id": mission_id,
        }
        with self._lock:
            entry["prev_hash"] = self._prev_hash
            entry["self_hash"] = self._compute_hash(entry)
            # Only advance _prev_hash if the write reaches disk. Otherwise
            # the next entry would link to a self_hash that never landed,
            # which verify_chain() reports as the wrong corruption point.
            if self._write_entry(entry):
                self._prev_hash = entry["self_hash"]
                return entry["self_hash"]
            return ""

    def verify_chain(self,
                     file_path: Path,
                     prev_hash: str = GENESIS_HASH
                     ) -> Tuple[bool, int]:
        """Verify a single file. Returns (valid, first_bad_line).

        first_bad_line is 0 when valid or when the file can't be opened
        (treated as opaque-corrupt with line=0). When verifying a rotated
        file, pass the previous file's tail self_hash as `prev_hash` so
        the chain links across rotation boundaries.
        """
        prev = prev_hash
        try:
            with file_path.open("r", encoding="utf-8") as f:
                for line_no, raw in enumerate(f, start=1):
                    if not raw.strip():
                        continue
                    entry = json.loads(raw)
                    if entry.get("self_hash") != self._compute_hash(entry):
                        return False, line_no
                    if entry.get("prev_hash") != prev:
                        return False, line_no
                    prev = entry["self_hash"]
        except (OSError, json.JSONDecodeError):
            return False, 0
        return True, 0

    def verify_chain_all(self,
                         files: Optional[List[Path]] = None
                         ) -> Tuple[bool, Optional[Path], int]:
        """Verify multiple rotated files in order, threading prev_hash
        across rotation boundaries.

        Returns (valid, first_bad_file, first_bad_line). When `files` is
        None, all `*.jsonl` files in `self.log_dir` are walked in sorted
        order.
        """
        if files is None:
            files = sorted(self.log_dir.glob("*.jsonl"))
        prev = GENESIS_HASH
        for path in files:
            valid, line_no = self.verify_chain(path, prev_hash=prev)
            if not valid:
                return False, path, line_no
            # Update prev to the tail self_hash of this file.
            try:
                with path.open("r", encoding="utf-8") as f:
                    last_hash = prev
                    for raw in f:
                        if not raw.strip():
                            continue
                        last_hash = json.loads(raw).get("self_hash", prev)
                    prev = last_hash
            except (OSError, json.JSONDecodeError):
                return False, path, 0
        return True, None, 0

    # ─── Internal helpers ───
    @staticmethod
    def _compute_hash(entry: Dict[str, Any]) -> str:
        """sha256 of canonical JSON, with self_hash field excluded."""
        canonical = {k: v for k, v in entry.items() if k != "self_hash"}
        s = json.dumps(canonical, sort_keys=True,
                       separators=(",", ":"))
        return hashlib.sha256(s.encode("utf-8")).hexdigest()

    def _write_entry(self, entry: Dict[str, Any]) -> bool:
        """Append one entry. Returns True on success, False on OSError.

        Mission protection: a write failure (disk full, FS readonly) never
        crashes the caller. But the failure is now observable via:
          - self.dropped_count (always)
          - self.on_drop callback (if supplied)
          - WARNING level log line
        so external monitoring (e.g. HealthMonitor) can react.
        """
        path = self._current_path()
        line = json.dumps(entry, sort_keys=True) + "\n"
        try:
            with path.open("a", encoding="utf-8") as f:
                f.write(line)
            self._current_size += len(line.encode())
            return True
        except OSError as e:
            self.dropped_count += 1
            log.warning("audit log write dropped (count=%d): %s",
                        self.dropped_count, e)
            if self.on_drop is not None:
                try:
                    self.on_drop(entry, e)
                except Exception as cb_exc:
                    log.error("audit on_drop callback raised: %s", cb_exc)
            return False

    def _current_path(self) -> Path:
        date_str = datetime.now(timezone.utc).strftime("%Y%m%d")
        if (self._current_file is not None
                and self._current_file.name.startswith(date_str)
                and self._current_size < self.MAX_FILE_BYTES):
            return self._current_file
        existing = sorted(self.log_dir.glob(f"{date_str}_*.jsonl"))
        idx = len(existing) + 1
        new_path = self.log_dir / f"{date_str}_{idx:04d}.jsonl"
        self._current_file = new_path
        self._current_size = 0
        return new_path


def publish_audit(queues,
                  category: str,
                  event: str,
                  actor: Optional[str] = None,
                  params: Optional[Dict[str, Any]] = None,
                  fsm_phase: Optional[str] = None,
                  mission_id: Optional[str] = None) -> None:
    """Publish an audit event onto the system bus.

    Producers across processes use this instead of constructing the dict
    inline so the writer in main (consuming `queues.audit_event`) sees a
    stable schema. The single-writer/many-publisher pattern keeps the
    hash chain consistent: only one AuditLogger.log() call site exists.
    """
    # Lazy import to avoid pulling ipc → audit_log into a cycle.
    from core.ipc import publish as _publish
    _publish(queues.audit_event, {
        "category": category,
        "event": event,
        "actor": actor,
        "params": params or {},
        "fsm_phase": fsm_phase,
        "mission_id": mission_id,
    })
