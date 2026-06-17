"""
DeadReckoning — IMU-based pose propagation during RTK outage.

The simplest defensible model that beats "use SLAM pose verbatim":

  • Track velocity in world frame: v_world
  • Position update:    x  ← x  + v_world · dt
  • Velocity update:    v  ← v  + (R · a_body − g) · dt
  • Orientation update: q  ← q ⊗ Δq(ω · dt)
  • Bias estimate (zero-velocity-update aware): when |a|, |ω| both small for
    > zupt_window_s, treat as stationary → average accel residual = bias.

Use:
  • Step at IMU rate (200 Hz from external IMU).
  • Reset(pose, vel, q) on each RTK Fixed/Float fix → cm/dm-level corrections.
  • Output `dead_reckon(now)` for LocalizationProcess to publish during outage.

Limitations (intentional, suitable for PoC):
  • No magnetometer → orientation drift not bounded by absolute heading
  • No EKF covariance update — uncertainty grows linearly with
    `time_since_reset × drift_per_s` (set by LocalizationProcess)
  • Body→world rotation uses zyx-order quaternion math
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

import numpy as np

GRAVITY = np.array([0.0, 0.0, 9.81], dtype=np.float64)


@dataclass
class DrConfig:
    zupt_acc_thresh:  float = 0.15      # m/s² total deviation considered "still"
    zupt_gyro_thresh: float = 0.05      # rad/s
    zupt_window_s:    float = 0.5
    max_dt_s:         float = 0.05      # >50 ms gap → zero out velocity (lost cycles)
    bias_alpha:       float = 0.02      # IIR for accel/gyro bias when ZUPT


def _quat_mul(q1: np.ndarray, q2: np.ndarray) -> np.ndarray:
    """Hamilton product of two quaternions (x, y, z, w)."""
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    return np.array([
        w1*x2 + x1*w2 + y1*z2 - z1*y2,
        w1*y2 - x1*z2 + y1*w2 + z1*x2,
        w1*z2 + x1*y2 - y1*x2 + z1*w2,
        w1*w2 - x1*x2 - y1*y2 - z1*z2,
    ], dtype=np.float64)


def _omega_to_dq(omega: np.ndarray, dt: float) -> np.ndarray:
    """Body-frame angular velocity → small-angle quaternion increment.

    For small θ = |ω·dt|: dq = (sin(θ/2)·ω̂, cos(θ/2)).
    """
    theta = np.linalg.norm(omega) * dt
    if theta < 1e-9:
        return np.array([0.0, 0.0, 0.0, 1.0])
    half = theta * 0.5
    s = np.sin(half) / theta * dt
    return np.array([omega[0] * s, omega[1] * s, omega[2] * s, np.cos(half)])


def _quat_to_R(q: np.ndarray) -> np.ndarray:
    """Quaternion (x, y, z, w) → 3×3 rotation matrix (body→world)."""
    x, y, z, w = q
    return np.array([
        [1 - 2*(y*y + z*z),     2*(x*y - z*w),     2*(x*z + y*w)],
        [    2*(x*y + z*w), 1 - 2*(x*x + z*z),     2*(y*z - x*w)],
        [    2*(x*z - y*w),     2*(y*z + x*w), 1 - 2*(x*x + y*y)],
    ], dtype=np.float64)


@dataclass
class DeadReckoner:
    """Stateful integrator. Caller drives: reset() then step() each IMU sample."""
    cfg: DrConfig = field(default_factory=DrConfig)
    # Pose state (world frame)
    pos: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float64))
    vel: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float64))
    q:   np.ndarray = field(default_factory=lambda: np.array([0, 0, 0, 1], dtype=np.float64))
    # Bias estimates (body frame)
    accel_bias: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float64))
    gyro_bias:  np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float64))
    # Internal
    last_t: Optional[float] = None
    zupt_start_t: Optional[float] = None
    distance_since_reset: float = 0.0

    def reset(self, pos: np.ndarray, q: np.ndarray,
              vel: Optional[np.ndarray] = None) -> None:
        """Snap state to a known truth (e.g., RTK fix reacquisition)."""
        self.pos = np.asarray(pos, dtype=np.float64).copy()
        self.q   = np.asarray(q,   dtype=np.float64).copy()
        # Renormalize quaternion to keep it valid
        n = np.linalg.norm(self.q)
        if n > 1e-9:
            self.q /= n
        self.vel = (np.asarray(vel, dtype=np.float64).copy()
                    if vel is not None else np.zeros(3, dtype=np.float64))
        self.last_t = None
        self.zupt_start_t = None
        self.distance_since_reset = 0.0

    def step(self, t: float, accel_body: np.ndarray, gyro: np.ndarray) -> None:
        """Integrate one IMU sample.

        accel_body : measured specific force in body frame, m/s²
                     (gravity NOT removed — measured value, includes -g·R)
        gyro       : angular velocity in body frame, rad/s
        """
        a = np.asarray(accel_body, dtype=np.float64) - self.accel_bias
        w = np.asarray(gyro,       dtype=np.float64) - self.gyro_bias

        if self.last_t is None:
            self.last_t = t
            self._zupt_update(t, a, w)
            return
        dt = t - self.last_t
        self.last_t = t
        if dt <= 0 or dt > self.cfg.max_dt_s:
            # Time gap too long — don't extrapolate velocity through it
            self.vel[:] = 0.0
            return

        # Orientation update (body-frame integration)
        dq = _omega_to_dq(w, dt)
        self.q = _quat_mul(self.q, dq)
        n = np.linalg.norm(self.q)
        if n > 1e-9:
            self.q /= n

        # Linear motion (subtract gravity to get true acceleration)
        R = _quat_to_R(self.q)
        a_world = R @ a - GRAVITY
        new_vel = self.vel + a_world * dt
        # Position via trapezoidal rule
        self.pos += 0.5 * (self.vel + new_vel) * dt
        self.distance_since_reset += float(
            np.linalg.norm(0.5 * (self.vel + new_vel) * dt))
        self.vel = new_vel

        # Bias learning when stationary
        self._zupt_update(t, a, w)

    def pose(self) -> tuple[np.ndarray, np.ndarray]:
        """Return (position, orientation) as float32 for messaging."""
        return self.pos.astype(np.float32), self.q.astype(np.float32)

    # ────────── Zero-velocity update (ZUPT) ──────────
    def _zupt_update(self, t: float, a: np.ndarray, w: np.ndarray) -> None:
        # Body-frame accel magnitude minus gravity should be ~0 when still
        a_dev = float(np.linalg.norm(a) - np.linalg.norm(GRAVITY))
        is_still = (abs(a_dev) < self.cfg.zupt_acc_thresh and
                    np.linalg.norm(w) < self.cfg.zupt_gyro_thresh)
        if is_still:
            if self.zupt_start_t is None:
                self.zupt_start_t = t
            elif (t - self.zupt_start_t) > self.cfg.zupt_window_s:
                # We've been still long enough — assume velocity is truly zero,
                # learn biases.
                self.vel[:] = 0.0
                # Body-frame stationary residual is a's bias (after gravity-comp)
                R = _quat_to_R(self.q)
                gravity_body = R.T @ GRAVITY
                expected_a = gravity_body                        # a should equal +g_body
                accel_residual = a - expected_a
                self.accel_bias += self.cfg.bias_alpha * accel_residual
                self.gyro_bias  += self.cfg.bias_alpha * w
        else:
            self.zupt_start_t = None
