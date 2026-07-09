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


# ─────────── Cost map (SAN v1.3 §6.4 + §9.6) ───────────
# Layer indices in a 4-layer master cost map. Order is FIXED — the
# compositor walks them in this order so OSM static → obstacle →
# traversability → inflation, with later layers able to OVER-write
# earlier ones (lethal always wins).
COST_LAYER_STATIC: int        = 0   # OSM static / SLAM persistent
COST_LAYER_OBSTACLE: int      = 1   # LiDAR returns above ground
COST_LAYER_TRAVERSABILITY: int = 2  # slope + ditch
COST_LAYER_INFLATION: int     = 3   # clearance around lethal cells

# Standard Nav2 cost-cell values. The master grid is uint8; downstream
# consumers compare against these constants rather than magic numbers.
# Two warn levels: LOW is the slope-style "you can drive this but slow
# down"; HIGH is the obstacle-style "this is almost certainly lethal,
# only the optimizer should consider it". COST_WARN is an alias for the
# high level so callers that don't care about the gradient stay readable.
COST_FREE: int        = 0
COST_WARN_LOW: int    = 100       # slope band — planner-traversable
COST_WARN_HIGH: int   = 200       # obstacle band — strongly discouraged
COST_WARN: int        = COST_WARN_HIGH
COST_LETHAL: int      = 254       # do-not-traverse
COST_UNKNOWN: int     = 255


@dataclass(slots=True)
class CostMapUpdate:
    """4-layer local cost map snapshot (SAN-IDS-CMD-001 v1.3 §5.13).

    Published at 1 Hz by MapFusionProcess. `master_payload` is the
    composited master grid (uint8, shape (H, W)) optionally PNG-encoded
    for cross-process transport; consumers reconstruct it via
    `mapping.cost_map.decode_master()`.
    """
    header: Header
    width: int                                # cells (columns)
    height: int                               # cells (rows)
    resolution_m: float                       # cell size (m)
    origin_xy: Tuple[float, float]            # world coords of grid (0, 0)
    # Composited master grid bytes — uint8 grid of COST_* values. Either
    # raw (encoding="raw") or PNG-deflated (encoding="png"). Empty for a
    # heartbeat-only update.
    master_payload: bytes = b""
    encoding: str = "raw"                     # raw | png
    # Number of cells per status — exposed for cheap consumer checks
    # without decoding the payload.
    n_lethal: int = 0
    n_warn: int = 0
    n_free: int = 0
    n_unknown: int = 0
    # Latency from sensor input to publish (s). Populated by the
    # producer; consumers can roll this into a histogram for KPP.
    producer_latency_s: float = 0.0


@dataclass(slots=True)
class OperatorAlert:
    """Operator-visible alert for non-emergency but actionable conditions
    (SAN v1.3 §6.4 — e.g. repeated cost-map avoidance failures).

    Distinct from EmergencyAlert: emergencies trip an immediate halt;
    operator alerts ask a human to look at the screen and decide.
    """
    header: Header
    code: str                         # "cost_map_avoidance_failed", ...
    description: str
    severity: str = "warning"         # info | warning | critical
    detail: dict = field(default_factory=dict)


@dataclass(slots=True)
class OperationState:
    """Periodic (1 Hz) heartbeat of the system's deployment posture.

    Carries the resolved SAN v1.3 §11 deployment_mode plus the swarm
    membership snapshot so the operator app, audit log, and any
    cross-process consumer can confirm the active policy without
    re-reading the yaml overlay tree.
    """
    header: Header
    deployment_mode: str = "production"        # see core.deployment.DeploymentMode
    robot_id: int = 0                          # this node's swarm id (1..MAX_ROBOTS, 0 = unset)
    robot_role: str = "follower"               # leader | follower | hub
    leader_robot_id: int = 1                   # convention: S1
    hub_robot_id: int = 2                      # convention: S2
    deputy_chain: Tuple[int, ...] = (2, 3, 4, 5, 6, 7, 8)
    n_alive_followers: int = 0


# Sharp interrupt-style alerts that need an immediate operator-visible
# notification, distinct from SafetyEvent's rolling-state codes. The
# v1.1 cliff detector (PHASE 7) is the first producer; future hazards
# (rollover detected by IMU integration, collision detected by LiDAR
# point cloud anomaly) will reuse the same envelope.
EMERGENCY_TYPE_CLIFF_DETECTED  = 1
EMERGENCY_TYPE_ROLLOVER        = 2   # reserved; future producer
EMERGENCY_TYPE_COLLISION       = 3   # reserved; future producer
EMERGENCY_TYPES = (
    EMERGENCY_TYPE_CLIFF_DETECTED,
    EMERGENCY_TYPE_ROLLOVER,
    EMERGENCY_TYPE_COLLISION,
)


@dataclass(slots=True)
class EmergencyAlert:
    """One-shot emergency notification.

    `type` is an int code (matches the wire format the operator UI
    consumes as `uint32`). `severity` is in [0.0, 1.0] — the producer
    picks; 0.9+ means "stop motion immediately." `description` is the
    operator-facing summary; production deployments may prepend a
    localised prefix downstream.
    """
    type: int = 0
    severity: float = 0.0
    description: str = ""
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.type not in EMERGENCY_TYPES:
            raise ValueError(f"unknown emergency type: {self.type}")
        if not 0.0 <= self.severity <= 1.0:
            raise ValueError(
                f"severity must be in [0, 1]: {self.severity}")
        if self.timestamp_ms < 0:
            raise ValueError(
                f"timestamp_ms must be non-negative: {self.timestamp_ms}")


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


# Surveillance sector assignment — leader → robot, on /swarm/surveillance/sector_assign.
# P1 RELIABLE, published every 10 s and on event (sector change, robot join/leave).
SECTOR_PRIORITY_PRIMARY      = "primary"
SECTOR_PRIORITY_THREAT_FOCUS = "threat_focus"
SECTOR_PRIORITY_OVERLAP      = "overlap"
# v1.5 (DCN-2026-001 D-001, SDD-SUR v1.5 §3.2): Deputy UGV shadow coverage.
# Deputy shares the Hub's rear-180° sector but sweeps with a 180° phase
# offset so the combined revisit rate over the rear band is doubled.
SECTOR_PRIORITY_SHADOW       = "shadow"
SECTOR_PRIORITIES = (
    SECTOR_PRIORITY_PRIMARY,
    SECTOR_PRIORITY_THREAT_FOCUS,
    SECTOR_PRIORITY_OVERLAP,
    SECTOR_PRIORITY_SHADOW,
)

SECTOR_MODE_SWEEP = "sweep"
SECTOR_MODE_TRACK = "track"
SECTOR_MODE_FIXED = "fixed"
SECTOR_MODES = (SECTOR_MODE_SWEEP, SECTOR_MODE_TRACK, SECTOR_MODE_FIXED)


@dataclass(slots=True)
class SectorAssign:
    """Leader-assigned surveillance sector for one robot.

    Angles are heading-relative degrees in [-180, 180]; the sector spans
    counter-clockwise from sector_start_deg to sector_end_deg in the robot
    body frame (0° = forward, +90° = left). valid_period_sec lets a
    follower drop stale assignments when the leader is unreachable.

    v1.5 (DCN-2026-001 D-001, SDD-SUR v1.5 §3.2): phase_offset_deg lets a
    SHADOW-priority robot (currently Deputy UGV S3) sweep the same sector
    as its principal (Hub UGV S2) but starting at a different sweep phase
    — typically 180° offset — to double the rear-band revisit rate.
    Non-SHADOW assignments have phase_offset_deg == 0.0.
    """
    sequence: int = 0
    robot_id: int = 0
    sector_start_deg: float = 0.0
    sector_end_deg: float = 0.0
    valid_period_sec: int = 10
    priority: str = SECTOR_PRIORITY_PRIMARY
    mode_hint: str = SECTOR_MODE_SWEEP
    phase_offset_deg: float = 0.0    # v1.5: SHADOW priority only; else 0.0
    timestamp_ms: int = 0

    def validate(self) -> None:
        """Raise ValueError on out-of-range or unknown enum values."""
        if not -180.0 <= self.sector_start_deg <= 180.0:
            raise ValueError(
                f"sector_start_deg out of range: {self.sector_start_deg}")
        if not -180.0 <= self.sector_end_deg <= 180.0:
            raise ValueError(
                f"sector_end_deg out of range: {self.sector_end_deg}")
        if self.valid_period_sec < 0:
            raise ValueError(
                f"valid_period_sec must be non-negative: {self.valid_period_sec}")
        if not -180.0 <= self.phase_offset_deg <= 180.0:
            raise ValueError(
                f"phase_offset_deg out of range: {self.phase_offset_deg}")
        if self.priority not in SECTOR_PRIORITIES:
            raise ValueError(f"unknown priority: {self.priority!r}")
        if self.mode_hint not in SECTOR_MODES:
            raise ValueError(f"unknown mode_hint: {self.mode_hint!r}")

    def is_expired(self, now_ms: int) -> bool:
        """True when (now - timestamp_ms) exceeds valid_period_sec."""
        if self.valid_period_sec <= 0:
            return False
        return (now_ms - self.timestamp_ms) > self.valid_period_sec * 1000


# Robot-internal pan-tilt command — perception/control → pan-tilt firmware.
# Local-only (no DDS topic); each robot derives its own command from the
# currently-active SectorAssign. PAN_TILT_MODE_TRACK retargets a moving
# AI detection, MODE_SWEEP runs an oscillation across `sweep_range_deg`,
# MODE_FIXED parks the head on (pan, tilt) without sweeping.
PAN_TILT_MODE_SWEEP  = "sweep"
PAN_TILT_MODE_TRACK  = "track"
PAN_TILT_MODE_FIXED  = "fixed"
# ENGAGE — head locked on a confirmed target, fire-permit imminent. Issued
# only by the leader's engagement controller (not by the follower's local
# sweep loop) and treated by the gimbal as a max-priority track that
# refuses pre-emption from a periodic sector refresh.
PAN_TILT_MODE_ENGAGE = "engage"
PAN_TILT_MODES = (
    PAN_TILT_MODE_SWEEP,
    PAN_TILT_MODE_TRACK,
    PAN_TILT_MODE_FIXED,
    PAN_TILT_MODE_ENGAGE,
)

# Hardware envelope from the gimbal datasheet — limits enforced here so a
# bogus command can't escape into the firmware. ±180° pan is the full
# circle; tilt is bounded by the chassis geometry.
PAN_TILT_PAN_LIMIT_DEG  = 180.0
PAN_TILT_TILT_MIN_DEG   = -30.0
PAN_TILT_TILT_MAX_DEG   = +90.0
PAN_TILT_MAX_SPEED_DPS  = 60.0


@dataclass(slots=True)
class PanTiltCommand:
    """Pan-tilt head pointing command derived from a SectorAssign.

    Angles are body-frame degrees (pan: 0° = forward, +90° = left; tilt:
    0° = horizon, +90° = up). `sweep_range_deg` is the peak-to-peak span
    the head should oscillate across when `mode == sweep`; in
    track/fixed/engage mode the value is ignored. `speed_dps` is the
    commanded angular speed and is clamped against the gimbal's hardware
    envelope. `sequence` lets a downstream gimbal driver discard
    out-of-order commands when the controller publishes faster than the
    actuator can settle.
    """
    sequence: int = 0
    robot_id: int = 0
    target_pan_deg: float = 0.0
    target_tilt_deg: float = 0.0
    speed_dps: float = 0.0
    mode: str = PAN_TILT_MODE_SWEEP
    sweep_range_deg: float = 0.0
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.mode not in PAN_TILT_MODES:
            raise ValueError(f"unknown pan-tilt mode: {self.mode!r}")
        if not -PAN_TILT_PAN_LIMIT_DEG <= self.target_pan_deg <= PAN_TILT_PAN_LIMIT_DEG:
            raise ValueError(
                f"target_pan_deg out of range: {self.target_pan_deg}")
        if not PAN_TILT_TILT_MIN_DEG <= self.target_tilt_deg <= PAN_TILT_TILT_MAX_DEG:
            raise ValueError(
                f"target_tilt_deg out of range: {self.target_tilt_deg}")
        if self.speed_dps < 0 or self.speed_dps > PAN_TILT_MAX_SPEED_DPS:
            raise ValueError(
                f"speed_dps out of range: {self.speed_dps}")
        if self.sweep_range_deg < 0 or self.sweep_range_deg > 360.0:
            raise ValueError(
                f"sweep_range_deg out of range: {self.sweep_range_deg}")


# Hub UGV SBC#1 → swarm aggregated map (/hub/slam/aggregated_map).
# P2 RELIABLE TRANSIENT_LOCAL, 30–60 s cadence. The Pose2D origin lets
# the consumer rotate the grid relative to the world frame; mp.Queue has
# no TRANSIENT_LOCAL equivalent, so the DDS/ROS bridge layer is
# responsible for late-joiner replay if/when it's wired.
@dataclass(slots=True)
class Pose2D:
    """Planar pose (x, y, yaw). Used by map metadata + lightweight 2D nav."""
    x: float = 0.0
    y: float = 0.0
    theta_rad: float = 0.0


@dataclass(slots=True)
class AggregatedMap:
    """Hub-fused global occupancy grid broadcast to the swarm.

    `occupancy_grid_png` is a PNG-encoded byte string of an 8-bit
    grayscale image where pixel value encodes occupancy
    (0 = free, 127 = unknown, 255 = occupied). Encoding is handled by
    mapping.aggregated_map so this dataclass stays a thin envelope.
    """
    sequence: int = 0
    occupancy_grid_png: bytes = b""
    origin: Pose2D = field(default_factory=Pose2D)
    resolution_m: float = 0.10
    width_cells: int = 0
    height_cells: int = 0
    contributing_robots: int = 0
    timestamp_ms: int = 0

    def validate(self) -> None:
        """Raise ValueError on internally-inconsistent metadata."""
        if self.resolution_m <= 0.0:
            raise ValueError(
                f"resolution_m must be positive: {self.resolution_m}")
        if self.width_cells < 0 or self.height_cells < 0:
            raise ValueError(
                f"width/height_cells must be non-negative: "
                f"{self.width_cells}x{self.height_cells}")
        if self.contributing_robots < 0:
            raise ValueError(
                f"contributing_robots must be non-negative: "
                f"{self.contributing_robots}")
        if self.width_cells > 0 and self.height_cells > 0 \
                and not self.occupancy_grid_png:
            raise ValueError(
                "occupancy_grid_png is empty but width/height_cells are set")

    def is_stale(self, now_ms: int, max_age_sec: float) -> bool:
        """True when (now - timestamp_ms) exceeds max_age_sec.

        Followers use this to drop a TRANSIENT_LOCAL late-joiner sample
        that is older than ~2× the Hub's broadcast cadence.
        """
        if max_age_sec <= 0:
            return False
        return (now_ms - self.timestamp_ms) > int(max_age_sec * 1000)


# Follower SLAM → Hub UGV SBC#1 local-delta broadcast.
# Topic: /{robot_id}/slam/local_delta — P2 RELIABLE, 30–60 s cadence.
# Replaces the v1.0 1 Hz SLAMDelta firehose: each follower aggregates its
# cells locally and emits a single PNG-compressed delta covering the
# `coverage_start_ms .. coverage_end_ms` window, reducing the swarm-mesh
# load by ~100×. The Hub SBC#1 fuses these into the AggregatedMap.
@dataclass(slots=True)
class SLAMLocalDelta:
    """Follower-side SLAM delta aggregated over a 30–60 s window.

    `occupancy_grid_delta_png` carries an 8-bit grayscale PNG where the
    middle value (127) signals "no change since last delta" — Hub-side
    fusion only writes the non-127 pixels into the global grid. The
    coverage window endpoints let the receiver order deltas from
    different followers when they arrive out of sequence on a lossy
    mesh.
    """
    sequence: int = 0
    robot_id: str = ""
    occupancy_grid_delta_png: bytes = b""
    origin: Pose2D = field(default_factory=Pose2D)
    resolution_m: float = 0.10
    coverage_start_ms: int = 0
    coverage_end_ms: int = 0
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.resolution_m <= 0.0:
            raise ValueError(
                f"resolution_m must be positive: {self.resolution_m}")
        if self.coverage_start_ms < 0 or self.coverage_end_ms < 0:
            raise ValueError(
                f"coverage timestamps must be non-negative: "
                f"{self.coverage_start_ms}..{self.coverage_end_ms}")
        if self.coverage_end_ms < self.coverage_start_ms:
            raise ValueError(
                f"coverage_end_ms < coverage_start_ms: "
                f"{self.coverage_end_ms} < {self.coverage_start_ms}")
        if not self.robot_id:
            raise ValueError("robot_id must be non-empty")

    def coverage_duration_ms(self) -> int:
        """Length of the window this delta covers."""
        return max(self.coverage_end_ms - self.coverage_start_ms, 0)


# Android APP → robot video stream request (/tablet/cmd/video_request).
# P1 RELIABLE. Pull-style — the tablet asks for a stream, the robot
# starts/stops the GStreamer pipeline. Action is one of:
#   • "start"           — bring up the pipeline at the requested quality
#   • "stop"            — tear it down
#   • "change_quality"  — keep the pipeline up, swap encoder bitrate/res
VIDEO_PROTOCOL_SRT = "srt"
VIDEO_PROTOCOL_UDP = "udp"
VIDEO_PROTOCOLS = (VIDEO_PROTOCOL_SRT, VIDEO_PROTOCOL_UDP)

VIDEO_CODEC_H265 = "h265"
VIDEO_CODEC_H264 = "h264"
VIDEO_CODECS = (VIDEO_CODEC_H265, VIDEO_CODEC_H264)

VIDEO_QUALITY_THUMBNAIL = "thumbnail"
VIDEO_QUALITY_LOW       = "low"
VIDEO_QUALITY_HD        = "hd"
VIDEO_QUALITY_FHD       = "fhd"
VIDEO_QUALITIES = (
    VIDEO_QUALITY_THUMBNAIL, VIDEO_QUALITY_LOW,
    VIDEO_QUALITY_HD, VIDEO_QUALITY_FHD,
)

VIDEO_ACTION_START          = "start"
VIDEO_ACTION_STOP           = "stop"
VIDEO_ACTION_CHANGE_QUALITY = "change_quality"
VIDEO_ACTIONS = (
    VIDEO_ACTION_START, VIDEO_ACTION_STOP, VIDEO_ACTION_CHANGE_QUALITY,
)


@dataclass(slots=True)
class VideoRequest:
    """Android tablet → robot video stream request.

    `passphrase_hint` is intentionally a hint (not the secret itself);
    the actual SRT passphrase / WiFi credential is negotiated over the
    encrypted control plane. An empty hint with `encryption=True` is
    valid and means "use whatever's been pre-shared."
    """
    sequence: int = 0
    target_robot_id: int = 0
    protocol: str = VIDEO_PROTOCOL_SRT
    desired_port: int = 0
    codec: str = VIDEO_CODEC_H265
    quality: str = VIDEO_QUALITY_HD
    encryption: bool = False
    passphrase_hint: str = ""
    action: str = VIDEO_ACTION_START
    timestamp_ms: int = 0

    def validate(self) -> None:
        """Raise ValueError on unknown enums or out-of-range port."""
        if self.protocol not in VIDEO_PROTOCOLS:
            raise ValueError(f"unknown protocol: {self.protocol!r}")
        if self.codec not in VIDEO_CODECS:
            raise ValueError(f"unknown codec: {self.codec!r}")
        if self.quality not in VIDEO_QUALITIES:
            raise ValueError(f"unknown quality: {self.quality!r}")
        if self.action not in VIDEO_ACTIONS:
            raise ValueError(f"unknown action: {self.action!r}")
        # Port 0 means "let the server pick"; valid IANA range is 1..65535.
        if not 0 <= self.desired_port <= 65535:
            raise ValueError(
                f"desired_port out of range: {self.desired_port}")


# Robot → tablet video stream response. The orchestrator builds one
# whenever the StreamingProcess reports a status transition; the WS
# layer relays it back to the app so the UI knows whether the requested
# stream is actually up.
VIDEO_STATUS_STREAMING = "streaming"
VIDEO_STATUS_STOPPED   = "stopped"
VIDEO_STATUS_ERROR     = "error"
VIDEO_STATUSES = (
    VIDEO_STATUS_STREAMING, VIDEO_STATUS_STOPPED, VIDEO_STATUS_ERROR,
)


@dataclass(slots=True)
class VideoResponse:
    """Robot → tablet status reply for a VideoRequest.

    Plays the v1.1 VideoStreamHandle role: `srt_uri` carries the fully
    formed GStreamer SRT URI (e.g.
    `srt://hub_ip:8888?mode=listener&latency=120&streamid=robot7`) so
    the tablet can plug it straight into a `srtsrc` element without
    re-deriving protocol+port semantics. The legacy `protocol`/`port`
    pair is retained for v1.0 peers that don't yet parse `srt_uri`;
    when present, both views must agree.

    `sequence` correlates back to the originating VideoRequest. `passphrase`
    is the actual mission-issued secret (not the hint) — empty when the
    stream is not encrypted. `stream_start_ms` is set on the first
    `streaming` transition and preserved across subsequent updates until
    the next stop/error.
    """
    sequence: int = 0
    robot_id: int = 0
    protocol: str = VIDEO_PROTOCOL_SRT
    port: int = 0
    srt_uri: str = ""
    codec: str = VIDEO_CODEC_H265
    quality: str = VIDEO_QUALITY_HD
    actual_bitrate_kbps: int = 0
    passphrase: str = ""
    status: str = VIDEO_STATUS_STOPPED
    error_msg: str = ""
    stream_start_ms: int = 0
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.protocol not in VIDEO_PROTOCOLS:
            raise ValueError(f"unknown protocol: {self.protocol!r}")
        if self.codec not in VIDEO_CODECS:
            raise ValueError(f"unknown codec: {self.codec!r}")
        if self.quality not in VIDEO_QUALITIES:
            raise ValueError(f"unknown quality: {self.quality!r}")
        if self.status not in VIDEO_STATUSES:
            raise ValueError(f"unknown status: {self.status!r}")
        if not 0 <= self.port <= 65535:
            raise ValueError(f"port out of range: {self.port}")
        if self.actual_bitrate_kbps < 0:
            raise ValueError(
                f"actual_bitrate_kbps must be non-negative: "
                f"{self.actual_bitrate_kbps}")
        if self.srt_uri and not self.srt_uri.lower().startswith(
                ("srt://", "udp://")):
            # Tablets need a parseable scheme. Empty srt_uri is legal
            # (v1.0 peer path); a non-empty value must be a real URI.
            raise ValueError(
                f"srt_uri scheme must be srt:// or udp://: {self.srt_uri!r}")
        # error_msg is only meaningful when status == error; we don't
        # enforce the inverse (an empty error_msg with status=error is
        # legal — sometimes the only thing the pipeline reports is
        # "playing=False").


# Hub UGV dual-SBC degraded-mode alerts. The Hub runs on two SBCs
# (SBC#1 = SLAM, SBC#2 = Comm); each watches the other via heartbeat.
# When a peer is silent past HEARTBEAT_TIMEOUT, the surviving SBC
# publishes a ThreatAlert so the swarm can enter partial-operation mode.
THREAT_SEVERITY_INFO     = "info"
THREAT_SEVERITY_WARNING  = "warning"
THREAT_SEVERITY_CRITICAL = "critical"
THREAT_SEVERITIES = (
    THREAT_SEVERITY_INFO, THREAT_SEVERITY_WARNING, THREAT_SEVERITY_CRITICAL,
)

# Threat type codes — small enum, 0..98 reserved for application
# threats (intruder, fire, etc.), 99+ for system / health events.
THREAT_TYPE_SBC_FAILED = 99


@dataclass(slots=True)
class ThreatAlert:
    """Hub UGV → swarm degraded-mode alert.

    `threat_type` is an int rather than a string so the wire format
    matches the ROS 2 spec the operator UI consumes (`uint32`). The
    Korean `message_ko` is the operator-facing text; English equivalents
    are not currently sent, but `peer_id` lets a localized UI build its
    own message.
    """
    severity: str = THREAT_SEVERITY_WARNING
    threat_type: int = THREAT_TYPE_SBC_FAILED
    message_ko: str = ""
    peer_id: str = ""
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.severity not in THREAT_SEVERITIES:
            raise ValueError(f"unknown severity: {self.severity!r}")
        if self.threat_type < 0:
            raise ValueError(f"threat_type must be non-negative: {self.threat_type}")


# Hub UGV dual-SBC failure summary (SAN-SDD-SUR-001 v1.1 §3.5).
#
# Published on /hub/swarm/health_summary as a typed companion to the
# ThreatAlert(SBC_FAILED) event: ThreatAlert is the *transition* signal
# the operator sees as a banner; SwarmHealthSummary is the *current*
# steady-state the swarm uses to gate degraded-mode routing decisions
# (e.g. "slam_sbc_failed=true → other robots assist with map fusion").
# Both flags carry the specific peer_id that timed out so a downstream
# consumer can disambiguate which SBC went silent.
@dataclass(slots=True)
class SwarmHealthSummary:
    """Hub-level rollup of dual-SBC failure state.

    `slam_sbc_failed`/`comm_sbc_failed` are latched booleans — true while
    the Hub watchdog has the peer marked stale, false after a recovery
    heartbeat. The Hub publishes on every transition (timeout OR
    recovery), so the consumer sees both edges without polling.
    """
    slam_sbc_failed: bool = False
    comm_sbc_failed: bool = False
    slam_sbc_peer_id: str = ""
    comm_sbc_peer_id: str = ""
    timestamp_ms: int = 0

    def validate(self) -> None:
        # A peer_id is required whenever the corresponding flag is set —
        # an unattributed failure is meaningless for the consumer's
        # routing-fallback logic.
        if self.slam_sbc_failed and not self.slam_sbc_peer_id:
            raise ValueError(
                "slam_sbc_failed=True requires non-empty slam_sbc_peer_id")
        if self.comm_sbc_failed and not self.comm_sbc_peer_id:
            raise ValueError(
                "comm_sbc_failed=True requires non-empty comm_sbc_peer_id")
        if self.timestamp_ms < 0:
            raise ValueError(
                f"timestamp_ms must be non-negative: {self.timestamp_ms}")


# Hub UGV SBC#2 → operator UI: Wi-Fi 6 mesh health snapshot.
# Topic: /hub/mesh/status — P1 RELIABLE, ~5 s cadence.
# Driven by a MeshMonitor that polls OpenWrt (batctl + mwan3) and
# publishes the rolled-up state for the operator console + downstream
# health-aware behavior (e.g. routing fallback when WAN flips to LTE).
@dataclass(slots=True)
class MeshStatus:
    """OpenWrt mesh + WAN failover state.

    `peer_count` is the count of batman-adv originators visible to this
    router (i.e. how many mesh peers we can route through, not counting
    ourselves). `wan_failover_active` is True iff the LTE WAN is up
    AND the primary (Wi-Fi 6 / wired) WAN is not — i.e. we're
    actively routing egress traffic over the cellular fallback.
    """
    peer_count: int = 0
    wan_primary_alive: bool = False
    wan_failover_active: bool = False
    mesh_id: str = ""
    timestamp_ms: int = 0

    def validate(self) -> None:
        if self.peer_count < 0:
            raise ValueError(
                f"peer_count must be non-negative: {self.peer_count}")
        if self.timestamp_ms < 0:
            raise ValueError(
                f"timestamp_ms must be non-negative: {self.timestamp_ms}")
        # Both flags True is self-inconsistent (LTE-only-failover but
        # primary still alive) — block it so a parser bug surfaces here
        # rather than confusing a downstream consumer.
        if self.wan_failover_active and self.wan_primary_alive:
            raise ValueError(
                "wan_failover_active=True is inconsistent with "
                "wan_primary_alive=True")


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
