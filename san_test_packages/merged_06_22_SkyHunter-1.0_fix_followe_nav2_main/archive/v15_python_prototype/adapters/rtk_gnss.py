"""
RTK GNSS adapter.

Hardware: u-blox ZED-F9P / Septentrio Mosaic-X5 / similar dual-band receivers
connected to RK3588 via USB-CDC or UART. Receives NMEA at 5–10 Hz.

Outputs RtkFix on `queues.rtk` at receiver rate. The localization process
is the only consumer; it decides whether to use this fix or fall back to
SLAM odometry based on fix_quality and age.

NTRIP split (SDD §3.2):
  NtripClientAdapter is its own process now. We:
    • subscribe to `queues.rtcm_corrections` (bytes) and write each frame
      to the GNSS serial port under self._serial_lock,
    • publish each $GxGGA we read on `queues.gga_latest` so the NTRIP
      adapter can forward it to the caster (VRS mountpoints need it).
  No direct NtripClient thread lives in this adapter anymore.

Falls back to STUB mode if no serial device is present (CI / dev box) —
publishes synthetic NMEA at 5 Hz so the pipeline can still be exercised.
"""
from __future__ import annotations

import logging
import math
import threading
import time
from typing import Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import (
    RTK_FIX_DGPS,
    RTK_FIX_FIXED,
    RTK_FIX_FLOAT,
    RTK_FIX_GPS,
    RTK_FIX_NONE,
    Header,
    RtkFix,
)

log = logging.getLogger(__name__)


# Map a config string ("gps", "dgps", "float", "fixed") to the integer
# constant. Stub mode publishes a synthetic track and we want operators
# to be able to choose how downstream localization treats it — defaulting
# to GPS-quality keeps DeadReckoner / SLAM odometry as the authoritative
# source in dev, instead of letting fake centimeter-grade fixes anchor
# the filter.
_STUB_QUALITY_BY_NAME = {
    "none":  RTK_FIX_NONE,
    "gps":   RTK_FIX_GPS,
    "dgps":  RTK_FIX_DGPS,
    "float": RTK_FIX_FLOAT,
    "fixed": RTK_FIX_FIXED,
}


# ────────── WGS84 → local ENU ──────────
_WGS84_A = 6378137.0
_WGS84_F = 1.0 / 298.257223563
_WGS84_E2 = _WGS84_F * (2.0 - _WGS84_F)


def _llh_to_ecef(lat_deg: float, lon_deg: float, alt_m: float):
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    sin_lat, cos_lat = math.sin(lat), math.cos(lat)
    n = _WGS84_A / math.sqrt(1.0 - _WGS84_E2 * sin_lat * sin_lat)
    x = (n + alt_m) * cos_lat * math.cos(lon)
    y = (n + alt_m) * cos_lat * math.sin(lon)
    z = (n * (1.0 - _WGS84_E2) + alt_m) * sin_lat
    return x, y, z


def llh_to_enu(lat: float, lon: float, alt: float,
               ref_lat: float, ref_lon: float, ref_alt: float
               ) -> np.ndarray:
    """Convert (lat, lon, alt) to local ENU meters, relative to reference."""
    x, y, z = _llh_to_ecef(lat, lon, alt)
    rx, ry, rz = _llh_to_ecef(ref_lat, ref_lon, ref_alt)
    dx, dy, dz = x - rx, y - ry, z - rz
    rl = math.radians(ref_lat)
    ro = math.radians(ref_lon)
    sin_l, cos_l = math.sin(rl), math.cos(rl)
    sin_o, cos_o = math.sin(ro), math.cos(ro)
    east  = -sin_o * dx + cos_o * dy
    north = -sin_l * cos_o * dx - sin_l * sin_o * dy + cos_l * dz
    up    =  cos_l * cos_o * dx + cos_l * sin_o * dy + sin_l * dz
    return np.array([east, north, up], dtype=np.float32)


# ────────── NMEA parsing (just GGA — enough for fix + altitude) ──────────
def _nmea_checksum_ok(line: str) -> bool:
    if "*" not in line:
        return False
    body, ck = line.lstrip("$").split("*", 1)
    s = 0
    for c in body:
        s ^= ord(c)
    try:
        return s == int(ck.strip(), 16)
    except ValueError:
        return False


def _parse_dm_to_deg(token: str, hemi: str) -> Optional[float]:
    """NMEA ddmm.mmmm → decimal degrees, signed.

    Returns None on malformed input (empty token, missing degree digits,
    non-numeric content). Earlier versions silently returned 0.0, which
    let bad sentences propagate as a fix at the (0, 0) coordinate origin.
    """
    if not token:
        return None
    dot = token.find(".")
    if dot < 2:
        return None
    try:
        deg = int(token[:dot - 2])
        minutes = float(token[dot - 2:])
    except ValueError:
        return None
    val = deg + minutes / 60.0
    return -val if hemi in ("S", "W") else val


def parse_gga(line: str) -> Optional[dict]:
    """Parse a single $GPGGA / $GNGGA line. Return None if invalid."""
    line = line.strip()
    if not line.startswith(("$GPGGA", "$GNGGA")):
        return None
    if not _nmea_checksum_ok(line):
        return None
    body = line.split("*")[0]
    f = body.split(",")
    if len(f) < 15:
        return None
    try:
        lat = _parse_dm_to_deg(f[2], f[3])
        lon = _parse_dm_to_deg(f[4], f[5])
        if lat is None or lon is None:
            return None
        quality = int(f[6] or 0)
        n_sat = int(f[7] or 0)
        hdop = float(f[8] or 99.9)
        alt = float(f[9] or 0.0)
    except (ValueError, IndexError):
        return None
    return dict(lat=lat, lon=lon, alt=alt,
                quality=quality, n_sat=n_sat, hdop=hdop)


# ────────── Adapter process ──────────
class RtkGnssAdapter(BaseProcess):
    """Reads RTK NMEA stream, publishes RtkFix at receiver rate.

    Optionally runs an NTRIP client thread that pushes RTCM corrections
    into the same serial port. Serial writes are guarded by a lock so
    NMEA parsing and RTCM injection don't collide.
    """

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="RtkGnssAdapter",
            shutdown_event=shutdown_event,
            rate_hz=10.0,
            cpu_affinity=config.get("system", "cpu_affinity", "rtk") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._serial = None
        self._serial_lock: Optional[threading.Lock] = None
        self._stub_mode = False
        self._stub_t0 = 0.0
        self._stub_quality = RTK_FIX_GPS
        self._seq = 0
        # Reference origin for ENU (set on first valid fix or from config)
        self._ref_lat = config.get("rtk", "reference_lat", default=None)
        self._ref_lon = config.get("rtk", "reference_lon", default=None)
        self._ref_alt = config.get("rtk", "reference_alt", default=None)

    def setup(self) -> None:
        device = self.cfg.get("rtk", "device", default="/dev/ttyACM0")
        baud   = self.cfg.get("rtk", "baud",   default=115200)
        self._serial_lock = threading.Lock()
        # Stub-quality config: accept "gps"/"dgps"/"float"/"fixed" or the
        # raw int. Anything else is logged and we keep the GPS default.
        sq = self.cfg.get("rtk", "stub_quality", default="gps")
        if isinstance(sq, str):
            sq_int = _STUB_QUALITY_BY_NAME.get(sq.lower())
            if sq_int is None:
                log.warning(f"unknown rtk.stub_quality {sq!r}; defaulting to 'gps'")
                sq_int = RTK_FIX_GPS
            self._stub_quality = sq_int
        elif isinstance(sq, int):
            self._stub_quality = sq
        try:
            import serial  # pyserial — optional dep
            self._serial = serial.Serial(device, baud, timeout=0.1)
            log.info(f"RTK serial open: {device} @ {baud}")
        except Exception as e:
            log.warning(f"RTK serial unavailable ({e}) → STUB mode")
            self._stub_mode = True
            self._stub_t0 = time.monotonic()

        # RTCM corrections from NtripClientAdapter — write to serial under
        # the same lock as our NMEA read so the two streams don't collide.
        if not self._stub_mode:
            self.spawn_thread(self._rtcm_consumer, name="RtcmCnsm")

    def _rtcm_consumer(self) -> None:
        """Drain queues.rtcm_corrections → GNSS receiver serial."""
        while self.is_running():
            frame = consume(self.queues.rtcm_corrections, timeout=0.5)
            if frame is None or self._serial is None:
                continue
            with self._serial_lock:
                try:
                    self._serial.write(frame)
                except Exception as e:
                    log.error(f"RTCM serial write failed: {e}")

    def step(self) -> None:
        if self._stub_mode:
            self._publish_stub()
            return
        # Read one NMEA line. Reads don't need the lock — the only writer
        # is the RTCM consumer thread, and pyserial is read/write-safe.
        try:
            raw = self._serial.readline().decode("ascii", errors="ignore")
        except Exception as e:
            log.error(f"RTK serial read error: {e}; switching to STUB")
            self._stub_mode = True
            self._stub_t0 = time.monotonic()
            return
        # Forward GGA to the NTRIP adapter (VRS support); regardless of fix
        # quality, NTRIP wants to know our approximate position.
        if raw.startswith("$GPGGA") or raw.startswith("$GNGGA"):
            line = raw if raw.endswith("\n") else raw + "\r\n"
            publish(self.queues.gga_latest, line)
        parsed = parse_gga(raw)
        if parsed is None:
            return
        self._publish_fix(**parsed)

    def teardown(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass

    # ─ helpers ─
    def _publish_fix(self, lat: float, lon: float, alt: float,
                     quality: int, n_sat: int, hdop: float) -> None:
        if quality == RTK_FIX_NONE:
            return        # don't pollute the stream with NO-FIX rows
        # Establish ENU reference on first usable fix
        if self._ref_lat is None and quality >= RTK_FIX_GPS:
            self._ref_lat, self._ref_lon, self._ref_alt = lat, lon, alt
            log.info(f"RTK ENU origin set to ({lat:.7f}, {lon:.7f}, {alt:.2f})")
        enu = (llh_to_enu(lat, lon, alt, self._ref_lat, self._ref_lon, self._ref_alt)
               if self._ref_lat is not None
               else np.zeros(3, dtype=np.float32))
        # Coarse uncertainty model: HDOP × per-quality scale
        scale = {
            RTK_FIX_FIXED: 0.02, RTK_FIX_FLOAT: 0.30,
            RTK_FIX_DGPS:  1.00, RTK_FIX_GPS:   3.00,
        }.get(quality, 99.0)
        sigma_xy = float(hdop * scale)
        msg = RtkFix(
            header=Header.now(frame_id="map", seq=self._seq),
            lat=lat, lon=lon, alt=alt, enu=enu,
            fix_quality=quality, n_satellites=n_sat,
            hdop=hdop, sigma_xy=sigma_xy, sigma_z=sigma_xy * 1.5,
        )
        self._seq += 1
        publish(self.queues.rtk, msg)

    def _publish_stub(self) -> None:
        """Emit a synthetic RTK-Fixed track — slow circle around origin."""
        t = time.monotonic() - self._stub_t0
        if self._ref_lat is None:
            self._ref_lat, self._ref_lon, self._ref_alt = 37.5665, 126.9780, 50.0  # Seoul
        radius = 5.0
        # Drift slowly so localization sees motion
        x_m = radius * math.cos(0.05 * t)
        y_m = radius * math.sin(0.05 * t)
        # Rough lat/lon derivative for the synthetic position
        dlat = y_m / 111_320.0
        dlon = x_m / (111_320.0 * math.cos(math.radians(self._ref_lat)))
        self._publish_fix(
            lat=self._ref_lat + dlat,
            lon=self._ref_lon + dlon,
            alt=self._ref_alt,
            quality=self._stub_quality, n_sat=18, hdop=0.6,
        )
