"""
Core message types — numpy arrays everywhere.

Design lessons applied:
  • Point clouds: np.ndarray (N, 4) float32, NEVER List[Tuple]
  • Occupancy grids: np.ndarray (H, W) float32
  • Headers: lightweight, __slots__ to reduce overhead
  • Large data (point clouds, images) passed by SharedMemory reference,
    not by value through mp.Queue.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import numpy as np


@dataclass(slots=True)
class Header:
    stamp: float = 0.0
    seq: int = 0
    frame_id: str = ""

    @staticmethod
    def now(frame_id: str = "", seq: int = 0) -> "Header":
        return Header(stamp=time.monotonic(), seq=seq, frame_id=frame_id)


# ─────────── Sensors ───────────
@dataclass(slots=True)
class LidarScanRef:
    """
    Reference to point cloud in SharedMemory.
    Actual data: shm_pool.read(shm_name) → np.ndarray (N, 4) float32 [x,y,z,intensity]
    """
    header: Header
    shm_name: str            # SharedMemory block name
    n_points: int
    ring_count: int = 16


@dataclass(slots=True)
class ImuData:
    header: Header
    linear_acc: np.ndarray = field(
        default_factory=lambda: np.zeros(3, dtype=np.float32))
    angular_vel: np.ndarray = field(
        default_factory=lambda: np.zeros(3, dtype=np.float32))
    orientation: np.ndarray = field(
        default_factory=lambda: np.array([0, 0, 0, 1], dtype=np.float32))


@dataclass(slots=True)
class CameraFrameRef:
    """JPEG/H.265 encoded frame in SharedMemory."""
    header: Header
    shm_name: str
    nbytes: int
    width: int
    height: int
    encoding: str = "h265"   # h265 | jpeg | rgb8


# ─────────── State estimation ───────────
@dataclass(slots=True)
class Pose6D:
    header: Header
    position: np.ndarray = field(
        default_factory=lambda: np.zeros(3, dtype=np.float32))   # x,y,z
    orientation: np.ndarray = field(
        default_factory=lambda: np.array([0, 0, 0, 1], dtype=np.float32))   # quaternion
    covariance: Optional[np.ndarray] = None   # (6,6) float32

    def xy(self) -> Tuple[float, float]:
        return float(self.position[0]), float(self.position[1])


@dataclass(slots=True)
class Twist6D:
    header: Header
    linear: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))
    angular: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))


# ─────────── Map (HD + SLAM + Fusion) ───────────
@dataclass(slots=True)
class MapTile:
    """
    Occupancy + confidence + temporal metadata.
    Cell layers (4-tier persistence) are tracked separately.
    """
    tile_id: str
    origin_xy: Tuple[float, float]   # bottom-left corner (m)
    size_m: float                    # side length (m)
    resolution: float                # cell size (m)
    occupancy: np.ndarray            # (H, W) float32, -1 unknown / 0 free / 1 occupied
    confidence: float = 1.0
    source: str = "slam"             # slam | fused
    last_update: float = 0.0
    # Temporal layer info (Q2 answer applied)
    persistence: Optional[np.ndarray] = None     # (H, W) float32 [0,1] permanence score
    first_seen: Optional[np.ndarray] = None      # (H, W) float32 timestamps
    last_seen: Optional[np.ndarray] = None


# ─────────── Mission / Locomotion ───────────
@dataclass(slots=True)
class Waypoint:
    id: str
    pose: Pose6D
    dwell_sec: float = 0.0
    checks: Tuple[str, ...] = ()         # ("ppe", "guardrail", ...)


@dataclass(slots=True)
class GoalPose:
    """High-level goal — RK3588 → Go2 (Nav2-compatible)."""
    header: Header
    position: np.ndarray              # x, y, z
    orientation: np.ndarray           # quaternion


@dataclass(slots=True)
class CmdVel:
    """Low-level velocity command (fallback)."""
    header: Header
    linear: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))
    angular: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))


# ─────────── Perception output ───────────
@dataclass(slots=True)
class Detection:
    label: str                       # "person_no_helmet", "open_hole", ...
    confidence: float
    bbox: np.ndarray                 # (4,) x1, y1, x2, y2
    pose_at_detect: Optional[Pose6D] = None


@dataclass(slots=True)
class AnomalyEvent:
    header: Header
    severity: str                    # "info" | "warning" | "critical"
    category: str                    # "ppe" | "hazard" | "intrusion" | ...
    description: str
    detections: List[Detection] = field(default_factory=list)
    image_ref: Optional[CameraFrameRef] = None


# ─────────── Robot status / safety ───────────
@dataclass(slots=True)
class RobotStatus:
    header: Header
    battery_soc: float = 1.0          # 0..1
    motor_temp_max: float = 0.0       # °C
    locomotion_mode: str = "stand"    # stand | walk | trot | docking | error
    fault_codes: Tuple[str, ...] = ()


@dataclass(slots=True)
class SafetyEvent:
    header: Header
    code: str                         # E1..E5 from operations scenario
    description: str
    suggested_action: str


# ═══════════════════════════════════════════════════════════════════════
# Multi-modal sensors on RK3588 (added Phase 2)
# ═══════════════════════════════════════════════════════════════════════

# RTK fix quality codes (matching NMEA GGA "Quality Indicator")
RTK_FIX_NONE     = 0          # No fix
RTK_FIX_GPS      = 1          # Standard GPS (~3 m)
RTK_FIX_DGPS     = 2          # Differential GPS (~1 m)
RTK_FIX_FLOAT    = 5          # RTK Float (~30 cm)
RTK_FIX_FIXED    = 4          # RTK Fixed (~2 cm)


@dataclass(slots=True)
class RtkFix:
    """RTK GNSS reading from u-blox / Septentrio / etc."""
    header: Header
    lat: float = 0.0                  # degrees (WGS84)
    lon: float = 0.0
    alt: float = 0.0                  # meters
    # Local ENU position relative to a fixed origin (set during platform init)
    enu: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))
    fix_quality: int = RTK_FIX_NONE
    n_satellites: int = 0
    hdop: float = 99.9                # horizontal dilution of precision
    # 1-σ position uncertainty in meters (cm-level when RTK Fixed)
    sigma_xy: float = 99.0
    sigma_z: float = 99.0

    def is_usable(self, max_age_s: float, now: float) -> bool:
        if (now - self.header.stamp) > max_age_s:
            return False
        return self.fix_quality in (RTK_FIX_FLOAT, RTK_FIX_FIXED)

    def is_high_precision(self) -> bool:
        return self.fix_quality == RTK_FIX_FIXED and self.sigma_xy < 0.10


@dataclass(slots=True)
class ThermalFrameRef:
    """16-bit thermal frame in SHM. Pixel value = centi-Kelvin or temp scale."""
    header: Header
    shm_name: str
    nbytes: int
    width: int
    height: int
    encoding: str = "mono16"          # mono16 (raw) or rgb8 (pseudocolor)
    min_temp_c: float = -40.0         # for visualization scaling
    max_temp_c: float = 150.0


@dataclass(slots=True)
class LrfReading:
    """Single-point laser range finder measurement (typ. 1 Hz)."""
    header: Header
    range_m: float = 0.0              # 0 = no return / out of range
    return_strength: float = 0.0      # 0..1 (manufacturer specific)
    valid: bool = False


@dataclass(slots=True)
class LocalizationStatus:
    """Which source is currently producing the localized pose.

    Published every cycle so mission/safety can react to degraded localization.
    """
    header: Header
    source: str = "init"              # rtk_fixed | rtk_float | odometry | dead_reckoning | init
    fallback_reason: str = ""         # populated when not RTK
    rtk_age_s: float = 99.0
    odom_drift_m: float = 0.0         # estimated since last good fix
    # Quaternion + xy uncertainty in meters (1-σ)
    sigma_xy: float = 99.0


# ═══════════════════════════════════════════════════════════════════════
# Swarm topics (SDD Rev.A.5 §6.7, §7.3)
# ═══════════════════════════════════════════════════════════════════════

# 5-Tier escape states (§6.7)
TIER_T0    = "T0"        # Predictive track (P_F_i fresh, leader visible)
TIER_T1_5  = "T1.5"      # Auto-reroute (≤2 m offset)
TIER_T1    = "T1"        # Normal PID following
TIER_T2    = "T2"        # Catch-up (1.2*d0 ≤ δ < 1.5*d0)
TIER_T3    = "T3"        # Fast catch-up (1.5*d0 ≤ δ < 2.0*d0)
TIER_T4    = "T4"        # Breadcrumb recovery (δ ≥ 2.0*d0)


@dataclass(slots=True)
class BreadcrumbPoint:
    """One sample on the leader's recent trajectory.

    Followers in T4 navigate this trail backwards-then-forwards to rejoin
    the leader when direct line-of-sight has been lost (SDD §6.7).
    """
    seq: int = 0
    stamp: float = 0.0
    x: float = 0.0
    y: float = 0.0
    yaw: float = 0.0


@dataclass(slots=True)
class FollowerTargetMessage:
    """1-second-lookahead target for one follower (SDD §7.3, 10 Hz).

    Generated by the leader's PredictiveLeader and consumed by the
    follower's local Nav2 planner. valid_until lets stale targets be
    rejected if a follower has been disconnected from the bus.
    """
    follower_id: str = ""
    stamp: float = 0.0
    valid_until: float = 0.0
    target_x: float = 0.0
    target_y: float = 0.0
    target_yaw: float = 0.0
    leader_seq: int = 0


@dataclass(slots=True)
class TierEvent:
    """A follower's tier state, published whenever it transitions.

    Used by mission/safety to log escape activity and by the leader's
    rollback policy to count `struggling` followers (T2/T3/T4).
    """
    header: Header
    follower_id: str = ""
    tier: str = TIER_T1
    delta_m: float = 0.0          # current along-path offset
    d0_m: float = 1.0             # nominal spacing
    reason: str = ""


@dataclass(slots=True)
class SharedMapChunk:
    """A map tile broadcast from leader to followers over the swarm mesh.

    Designed to fit in one DDS message (≤ 64 kB). Chunked for large maps.
    """
    header: Header
    map_id: str
    chunk_id: int
    n_chunks: int
    # Pre-compressed occupancy grid bytes (zlib of int8 -1/0/100 array)
    payload: bytes = b""
    origin_xy: np.ndarray = field(default_factory=lambda: np.zeros(2, dtype=np.float32))
    resolution_m: float = 0.10


# ═══════════════════════════════════════════════════════════════════════
# LTE modem — fallback uplink when WiFi6 mesh / LAN is unavailable
# ═══════════════════════════════════════════════════════════════════════

# LTE registration states (3GPP TS 27.007 §7.2)
LTE_NOT_REGISTERED = 0
LTE_REGISTERED_HOME = 1
LTE_SEARCHING = 2
LTE_REGISTRATION_DENIED = 3
LTE_UNKNOWN = 4
LTE_REGISTERED_ROAMING = 5


@dataclass(slots=True)
class LteStatus:
    """Cellular link status from a Quectel/Telit/SIMCom modem.

    Used by:
      • CommProcess  — decide when to fail over from WiFi6 to LTE for uploads
      • Safety       — alert if BOTH WiFi6 and LTE are degraded
      • Mission      — adjust evidence upload behavior under low bandwidth
    """
    header: Header
    registered: int = LTE_NOT_REGISTERED
    operator: str = ""                # PLMN, e.g. "KT", "SKT", "Vodafone"
    rat: str = ""                     # "LTE", "5G-NSA", "5G-SA"
    rsrp_dbm: float = -140.0          # signal strength (-140..-44 typical)
    rsrq_db: float = -20.0            # signal quality
    sinr_db: float = -20.0
    band: int = 0
    cell_id: int = 0
    # Data plane
    pdp_active: bool = False          # data session up?
    ip_address: str = ""              # "" until PDP active
    apn: str = ""
    # Metering — to throttle non-critical uploads
    bytes_tx_today: int = 0
    bytes_rx_today: int = 0
