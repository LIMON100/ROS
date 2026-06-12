"""M4 assault mode (P2-8, user decision #7).

User decision #7: 돌격 = d=15m, theta=60° (vs Recon d=5m).

Behavior tree integration: validates assault config + emits
formation_change command to PredictivePlanner.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class AssaultConfig:
    d_m: float = 15.0
    theta_deg: float = 60.0
    leader_max_speed_mps: float = 1.3
    max_duration_s: int = 600
    formation_type: str = "V_SHAPE"


class AssaultMissionValidator:
    """Validate M4_assault mission configuration."""

    EXPECTED_D_M = 15.0
    EXPECTED_THETA_DEG = 60.0
    SPEED_CAP_MPS = 1.3

    @classmethod
    def validate(cls, config: dict) -> tuple[bool, str]:
        """Returns (valid, error_msg)."""
        if config.get("type") != "assault":
            return False, "type must be 'assault'"
        formation = config.get("formation", {})
        if formation.get("d_m") != cls.EXPECTED_D_M:
            return False, f"d_m must be {cls.EXPECTED_D_M} (user decision #7)"
        if formation.get("theta_deg") != cls.EXPECTED_THETA_DEG:
            return False, f"theta_deg must be {cls.EXPECTED_THETA_DEG}"
        if config.get("leader_max_speed_mps", 0) > cls.SPEED_CAP_MPS:
            return False, f"leader_max_speed_mps must be ≤ {cls.SPEED_CAP_MPS}"
        return True, "OK"

    @classmethod
    def from_yaml_dict(cls, mission_dict: dict) -> AssaultConfig:
        f = mission_dict.get("formation", {})
        return AssaultConfig(
            d_m=f.get("d_m", 15.0),
            theta_deg=f.get("theta_deg", 60.0),
            leader_max_speed_mps=mission_dict.get("leader_max_speed_mps", 1.3),
            max_duration_s=mission_dict.get("max_duration_s", 600),
            formation_type=f.get("type", "V_SHAPE"),
        )
