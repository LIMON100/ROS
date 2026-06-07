"""Pan-tilt head controller — SectorAssign → PanTiltCommand.

Each robot derives its own pan-tilt motion from the surveillance sector
currently assigned to it by the swarm leader. This module is pure
compute: callers feed it SectorAssign updates and track callbacks, and
get back PanTiltCommand messages ready to publish on the
`pantilt_command` queue.

Sweep speed comes from the geometric relation
    ω_sweep = (sector_width - HFOV) × 2 / period
i.e. the head traverses the *uncovered* portion of the sector twice per
valid_period_sec so the full sector is scanned end-to-end-and-back. When
sector_width ≤ HFOV the head parks at sector center (speed = 0).
"""
from __future__ import annotations

import time
from typing import Optional

from core.messages import (
    PAN_TILT_MAX_SPEED_DPS,
    PAN_TILT_MODE_ENGAGE,
    PAN_TILT_MODE_FIXED,
    PAN_TILT_MODE_SWEEP,
    PAN_TILT_MODE_TRACK,
    PAN_TILT_PAN_LIMIT_DEG,
    PAN_TILT_TILT_MAX_DEG,
    PAN_TILT_TILT_MIN_DEG,
    SECTOR_MODE_FIXED,
    SECTOR_MODE_SWEEP,
    SECTOR_MODE_TRACK,
    PanTiltCommand,
    SectorAssign,
)
from swarm.sector_assign import sector_center_deg, sector_width_deg

# Mapping from SectorAssign.mode_hint → PanTiltCommand.mode. The follower
# obeys the leader's mode hint unless the gimbal cannot fulfil it (e.g.
# `track` requested without a local detection — falls back to `sweep`).
_SECTOR_TO_PANTILT_MODE = {
    SECTOR_MODE_SWEEP: PAN_TILT_MODE_SWEEP,
    SECTOR_MODE_TRACK: PAN_TILT_MODE_TRACK,
    SECTOR_MODE_FIXED: PAN_TILT_MODE_FIXED,
}

# Default HFOV of the imx678 + thermal payload. RGB sensor is the limiting
# factor (~60°); thermal is wider (~75°) so an RGB-centric sweep also
# covers the thermal field. Operators can override via config.
DEFAULT_HFOV_DEG = 60.0


def sweep_speed_dps(
    sector_width_deg_: float,
    period_sec: int,
    hfov_deg: float = DEFAULT_HFOV_DEG,
    max_speed_dps: float = PAN_TILT_MAX_SPEED_DPS,
) -> float:
    """Compute commanded sweep speed for a sector.

    Returns 0 when the sector fits inside the camera HFOV (no sweeping
    needed). Clamps to `max_speed_dps` so a very tight period on a wide
    sector doesn't outrun the gimbal.
    """
    uncovered = max(float(sector_width_deg_) - float(hfov_deg), 0.0)
    period = max(int(period_sec), 1)
    speed = (uncovered * 2.0) / period
    return min(speed, float(max_speed_dps))


def _clamp_tilt(tilt_deg: float) -> float:
    return max(PAN_TILT_TILT_MIN_DEG, min(PAN_TILT_TILT_MAX_DEG, float(tilt_deg)))


def _clamp_pan(pan_deg: float) -> float:
    return max(-PAN_TILT_PAN_LIMIT_DEG,
               min(PAN_TILT_PAN_LIMIT_DEG, float(pan_deg)))


class PanTiltController:
    """Drives the pan-tilt head from sector assignments + track callbacks.

    The controller is intentionally stateless beyond `_last_command` so
    consumers can poll the most recent command without re-deriving it.
    Threading: methods are not internally synchronized — wrap with a
    lock if multiple threads call into the same instance.
    """

    def __init__(
        self,
        robot_id: int,
        hfov_deg: float = DEFAULT_HFOV_DEG,
        default_tilt_deg: float = 0.0,
        max_speed_dps: float = PAN_TILT_MAX_SPEED_DPS,
    ):
        self.robot_id = int(robot_id)
        self.hfov_deg = float(hfov_deg)
        self.default_tilt_deg = _clamp_tilt(default_tilt_deg)
        self.max_speed_dps = float(max_speed_dps)
        self._last_command: Optional[PanTiltCommand] = None
        # Monotonically increasing per-instance sequence number — lets
        # a downstream gimbal driver discard out-of-order frames when
        # sweep + track callbacks race.
        self._sequence: int = 0

    def _next_sequence(self) -> int:
        self._sequence += 1
        return self._sequence

    @property
    def last_command(self) -> Optional[PanTiltCommand]:
        return self._last_command

    def on_sector(
        self,
        msg: SectorAssign,
        now_ms: Optional[int] = None,
    ) -> Optional[PanTiltCommand]:
        """Translate a SectorAssign into a sweep/fixed pan-tilt command.

        Returns None if the message is for a different robot. A sector
        with mode_hint=track but no tracked object resolves to a sweep
        in the same sector (the AI track loop will call `on_track` once
        it has a bearing).
        """
        if msg.robot_id != self.robot_id:
            return None
        width = sector_width_deg(msg.sector_start_deg, msg.sector_end_deg)
        center = sector_center_deg(msg.sector_start_deg, msg.sector_end_deg)
        if width <= self.hfov_deg:
            mode = PAN_TILT_MODE_FIXED
        else:
            mode = _SECTOR_TO_PANTILT_MODE.get(msg.mode_hint, PAN_TILT_MODE_SWEEP)
            # Track without a bearing degenerates to sweep — see on_track.
            if mode == PAN_TILT_MODE_TRACK:
                mode = PAN_TILT_MODE_SWEEP
        if mode == PAN_TILT_MODE_FIXED:
            # Fixed mode parks the head; sweep speed is meaningless.
            speed = 0.0
        else:
            speed = sweep_speed_dps(
                width, msg.valid_period_sec,
                hfov_deg=self.hfov_deg,
                max_speed_dps=self.max_speed_dps,
            )
        cmd = PanTiltCommand(
            sequence=self._next_sequence(),
            robot_id=self.robot_id,
            target_pan_deg=_clamp_pan(center),
            target_tilt_deg=self.default_tilt_deg,
            speed_dps=speed,
            mode=mode,
            sweep_range_deg=max(width - self.hfov_deg, 0.0),
            timestamp_ms=now_ms if now_ms is not None else _now_ms(),
        )
        cmd.validate()
        self._last_command = cmd
        return cmd

    def on_engage(
        self,
        bearing_deg: float,
        tilt_deg: Optional[float] = None,
        speed_dps: Optional[float] = None,
        now_ms: Optional[int] = None,
    ) -> PanTiltCommand:
        """Lock the head on a confirmed fire-permit target.

        Engage commands are emitted only by the leader's engagement
        controller — never by the local sweep loop — and the gimbal
        treats them as max-priority: a subsequent sector refresh will
        not pre-empt an engage until the leader publishes a follow-up
        sweep/fixed command. Behaviorally identical to track except for
        the mode tag, which signals the priority contract downstream.
        """
        cmd = PanTiltCommand(
            sequence=self._next_sequence(),
            robot_id=self.robot_id,
            target_pan_deg=_clamp_pan(bearing_deg),
            target_tilt_deg=_clamp_tilt(
                tilt_deg if tilt_deg is not None else self.default_tilt_deg),
            speed_dps=min(
                float(speed_dps) if speed_dps is not None
                else self.max_speed_dps,
                self.max_speed_dps),
            mode=PAN_TILT_MODE_ENGAGE,
            sweep_range_deg=0.0,
            timestamp_ms=now_ms if now_ms is not None else _now_ms(),
        )
        cmd.validate()
        self._last_command = cmd
        return cmd

    def on_track(
        self,
        bearing_deg: float,
        tilt_deg: Optional[float] = None,
        speed_dps: Optional[float] = None,
        now_ms: Optional[int] = None,
    ) -> PanTiltCommand:
        """Slew the head to a tracked object's bearing.

        Used when the AI detector emits a fresh bearing — overrides the
        last sweep command until the next SectorAssign tick. Speed
        defaults to the gimbal's maximum so the head catches up quickly;
        the caller can pass a smaller value for smooth-tracking.
        """
        cmd = PanTiltCommand(
            sequence=self._next_sequence(),
            robot_id=self.robot_id,
            target_pan_deg=_clamp_pan(bearing_deg),
            target_tilt_deg=_clamp_tilt(
                tilt_deg if tilt_deg is not None else self.default_tilt_deg),
            speed_dps=min(
                float(speed_dps) if speed_dps is not None
                else self.max_speed_dps,
                self.max_speed_dps),
            mode=PAN_TILT_MODE_TRACK,
            sweep_range_deg=0.0,
            timestamp_ms=now_ms if now_ms is not None else _now_ms(),
        )
        cmd.validate()
        self._last_command = cmd
        return cmd


def _now_ms() -> int:
    return int(time.time() * 1000)


__all__ = (
    "DEFAULT_HFOV_DEG",
    "PanTiltController",
    "sweep_speed_dps",
)
