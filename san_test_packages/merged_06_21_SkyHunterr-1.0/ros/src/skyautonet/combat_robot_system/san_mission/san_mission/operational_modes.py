# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 9-10 — Operational mode controller.

Direct port of mission/operational_modes.py per DCN-2026-002 D-007.

5 standard operational mode presets (SDD Rev.A.6 §7.1, user decision #7):
  • dev_test  — 3 m spacing, capped at 1.0 m/s
  • narrow    — 3 m spacing, narrow corridor / forest
  • recon     — 5 m spacing, default mode, 360° coverage
  • wide      — 7 m spacing, dispersed surveillance
  • assault   — 15 m spacing, 122mm-radius avoidance

No ROS dependencies — fully pytest-able standalone.

PATCH 2026-05-13 (san_mission deep-dive C5):
  * threading.Lock protects the `current` field. The mission_node
    rclpy executor (which may be Multi-Threaded) can have one
    callback request a mode while another reads get_current_preset().
    The GIL guarantees attribute reads/writes are atomic, but
    compound operations (request_mode does check + write) are not.
    The lock makes the full request_mode atomic.

DCN-2026-023 v2 (2026-05-23):
  * PIN authentication mechanism removed. The only production caller
    was the BLE 0xFF05 GATT challenge, which DCN-2026-008 deleted.
    Operator app now connects exclusively via Wi-Fi
    (rosbridge_server).

DCN-2026-024 (2026-05-24) — Option C: Mode-only gate:
  * Restores an authentication gate specifically for DEV_TEST mode
    (the only elevated mode in the current preset set). The gate is
    a shared-secret token comparison — operator app must present the
    token alongside the request to enter DEV_TEST. Other modes (NARROW
    / RECON / WIDE / ASSAULT) remain unauthenticated.
  * Secret source priority:
      1. ctor parameter `dev_test_secret` (test injection)
      2. env var `SAN_DEV_TEST_SECRET`
      3. file `/etc/san/dev_test_secret` (mode 0400 recommended)
      4. none → DEV_TEST is locked out entirely (fail-closed)
  * Token comparison uses `secrets.compare_digest` for
    constant-time equality.
  * Replay protection (nonce + timestamp) is out of scope for v1;
    a follow-up DCN can extend the gate to a challenge-response
    handshake if the WiFi channel itself is not trusted.
"""
from __future__ import annotations

import os
import secrets
import threading
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional, Tuple


class OperationalMode(str, Enum):
    DEV_TEST = "dev_test"
    NARROW   = "narrow"
    RECON    = "recon"          # default
    WIDE     = "wide"
    ASSAULT  = "assault"


@dataclass(frozen=True)
class ModePreset:
    name: str
    d_m: float                        # follower spacing (m)
    theta_deg: float                  # V-shape angle (deg)
    leader_max_speed_mps: float
    description: str = ""


PRESETS: dict[OperationalMode, ModePreset] = {
    OperationalMode.DEV_TEST: ModePreset(
        name="개발자/Test",
        d_m=3.0,
        theta_deg=60.0,
        leader_max_speed_mps=1.0,        # forced 1.0 m/s
        description="실내/단거리 시험.",
    ),
    OperationalMode.NARROW: ModePreset(
        name="협로 침투",
        d_m=3.0,
        theta_deg=40.0,
        leader_max_speed_mps=1.3,
        description="좁은 회랑/숲 통과. 통과폭 ~4.1 m.",
    ),
    OperationalMode.RECON: ModePreset(
        name="정찰/방어 (이동)",
        d_m=5.0,
        theta_deg=90.0,
        leader_max_speed_mps=1.3,
        description="기본 모드. 360° 시야 확보. 통과폭 ~14.1 m.",
    ),
    OperationalMode.WIDE: ModePreset(
        name="광역 정찰",
        d_m=7.0,
        theta_deg=120.0,
        leader_max_speed_mps=1.3,
        description="분산 감시. 통과폭 ~24.2 m.",
    ),
    OperationalMode.ASSAULT: ModePreset(
        name="돌격",
        d_m=15.0,
        theta_deg=60.0,
        leader_max_speed_mps=1.3,
        description="전방 화력 + 122 mm 살상반경 회피. 통과폭 ~30 m.",
    ),
}


class OperationalModeController:
    """Apply a mode preset.

    PATCH 2026-05-13 (C5): thread-safe — all state mutations + reads
    serialized by an internal lock.

    DCN-2026-024 (Option C): DEV_TEST mode requires a shared-secret
    auth token. Other modes are unauthenticated.
    """

    DEFAULT_SECRET_PATH = "/etc/san/dev_test_secret"
    ENV_VAR             = "SAN_DEV_TEST_SECRET"

    def __init__(self,
                 dev_test_secret: Optional[str] = None,
                 dev_test_secret_path: Optional[str] = None):
        """
        Args:
            dev_test_secret:      DCN-024 — DEV_TEST gate token. If None,
                                  falls back to SAN_DEV_TEST_SECRET env
                                  var, then to `dev_test_secret_path`.
                                  When all three sources yield None,
                                  DEV_TEST is locked out (fail-closed).
            dev_test_secret_path: file path for the secret. Default
                                  /etc/san/dev_test_secret. Pass "" to
                                  disable file lookup.
        """
        self._lock = threading.Lock()
        self.current: OperationalMode = OperationalMode.RECON
        self._dev_test_secret = self._resolve_secret(
            dev_test_secret,
            dev_test_secret_path
            if dev_test_secret_path is not None
            else self.DEFAULT_SECRET_PATH)

    @classmethod
    def _resolve_secret(cls,
                        explicit: Optional[str],
                        path: str) -> Optional[str]:
        """Priority: explicit ctor arg → env var → file. Returns None
        when no source yields a non-empty value (fail-closed)."""
        if explicit:
            return explicit
        env_val = os.environ.get(cls.ENV_VAR)
        if env_val:
            return env_val
        if path:
            try:
                content = Path(path).read_text(encoding="utf-8").strip()
                if content:
                    return content
            except (FileNotFoundError, PermissionError, OSError):
                pass
        return None

    def dev_test_secret_loaded(self) -> bool:
        """Test/diagnostics: returns True iff a secret was resolved
        at ctor time. Does NOT leak the secret itself."""
        return self._dev_test_secret is not None

    def request_mode(
            self,
            mode: OperationalMode,
            auth_token: str = "") -> Tuple[bool, str]:
        """Return (success, message). Idempotent.

        DCN-2026-024: when `mode == DEV_TEST`, `auth_token` MUST equal
        the configured secret (constant-time compare). Other modes
        ignore `auth_token`.
        """
        preset = PRESETS.get(mode)
        if preset is None:
            return False, f"unknown mode: {mode}"

        # ─── DCN-2026-024 — DEV_TEST gate (Option C) ────────────────
        if mode == OperationalMode.DEV_TEST:
            if self._dev_test_secret is None:
                return False, (
                    "dev_test requires SAN_DEV_TEST_SECRET / "
                    "/etc/san/dev_test_secret — none configured")
            # constant-time compare to defeat token-recovery timing
            # side-channels.
            if not auth_token or not secrets.compare_digest(
                    auth_token, self._dev_test_secret):
                return False, "dev_test auth token mismatch"

        with self._lock:
            self.current = mode
            return True, f"mode set to {mode.value}"

    def get_current_preset(self) -> ModePreset:
        with self._lock:
            return PRESETS[self.current]

    def get_max_speed(self) -> float:
        """Per-preset speed cap. DEV_TEST is 1.0 m/s regardless of any
        formation override coming through other channels."""
        with self._lock:
            return PRESETS[self.current].leader_max_speed_mps
