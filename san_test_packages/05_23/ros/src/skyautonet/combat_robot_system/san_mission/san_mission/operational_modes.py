"""SAN v1.5 Phase 2-E Turn 9-10 — Operational mode controller.

Direct port of mission/operational_modes.py per DCN-2026-002 D-007.

5 standard operational mode presets (SDD Rev.A.6 §7.1, user decision #7):
  • dev_test  — 3 m spacing, PIN-gated, capped at 1.0 m/s
  • narrow    — 3 m spacing, narrow corridor / forest
  • recon     — 5 m spacing, default mode, 360° coverage
  • wide      — 7 m spacing, dispersed surveillance
  • assault   — 15 m spacing, 122mm-radius avoidance

No ROS dependencies — fully pytest-able standalone.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Tuple


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
    requires_pin: bool = False
    description: str = ""


PRESETS: dict[OperationalMode, ModePreset] = {
    OperationalMode.DEV_TEST: ModePreset(
        name="개발자/Test",
        d_m=3.0,
        theta_deg=60.0,
        leader_max_speed_mps=1.0,        # forced 1.0 m/s
        requires_pin=True,
        description="실내/단거리 시험. PIN auth required.",
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
    """Apply a mode preset, gated by PIN authentication for DEV_TEST.

    DEV_TEST requires the operator to have completed the 0xFF05 PIN
    challenge this BLE session — call set_pin_authenticated(True)
    before request_mode(DEV_TEST).
    """

    def __init__(self):
        self.current: OperationalMode = OperationalMode.RECON
        self._pin_authenticated: bool = False

    def set_pin_authenticated(self, authenticated: bool) -> None:
        self._pin_authenticated = bool(authenticated)

    def is_pin_authenticated(self) -> bool:
        return self._pin_authenticated

    def request_mode(self,
                     mode: OperationalMode) -> Tuple[bool, str]:
        """Return (success, message). Idempotent."""
        preset = PRESETS.get(mode)
        if preset is None:
            return False, f"unknown mode: {mode}"
        if preset.requires_pin and not self._pin_authenticated:
            return False, (
                f"{mode.value} requires PIN authentication (BLE 0xFF05)")
        self.current = mode
        return True, f"mode set to {mode.value}"

    def get_current_preset(self) -> ModePreset:
        return PRESETS[self.current]

    def get_max_speed(self) -> float:
        """Per-preset speed cap. DEV_TEST is 1.0 m/s regardless of any
        formation override coming through other channels."""
        return PRESETS[self.current].leader_max_speed_mps
