"""
Tests for NMEA parsing and WGS84→ENU conversion.

Pure-function tests — no serial port, no multiprocessing.
"""
from __future__ import annotations

import pytest

from adapters.rtk_gnss import (
    _nmea_checksum_ok,
    _parse_dm_to_deg,
    llh_to_enu,
    parse_gga,
)
from core.messages import RTK_FIX_FIXED


# ───────────── NMEA checksum ─────────────
def test_checksum_validates_real_message():
    line = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
    assert _nmea_checksum_ok(line)


def test_checksum_detects_corruption():
    line = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48"
    assert not _nmea_checksum_ok(line)


def test_checksum_rejects_missing_star():
    assert not _nmea_checksum_ok("$GPGGA,no_star_here")


# ───────────── DM → decimal degrees ─────────────
def test_dm_to_deg_basic():
    # 4807.038' = 48° 07.038' → 48.1173°
    val = _parse_dm_to_deg("4807.038", "N")
    assert val == pytest.approx(48.1173, abs=1e-4)


def test_dm_to_deg_southern_hemisphere_negative():
    val = _parse_dm_to_deg("4807.038", "S")
    assert val == pytest.approx(-48.1173, abs=1e-4)


def test_dm_to_deg_western_hemisphere_negative():
    val = _parse_dm_to_deg("12658.68", "W")
    assert val == pytest.approx(-126.978, abs=1e-3)


def test_dm_to_deg_empty_returns_none():
    # Empty / malformed tokens used to silently return 0.0, which let
    # garbage NMEA propagate as a fix at the (0, 0) coordinate origin.
    assert _parse_dm_to_deg("", "N") is None


def test_dm_to_deg_malformed_returns_none():
    assert _parse_dm_to_deg("not-a-number", "N") is None
    assert _parse_dm_to_deg("4.7", "N") is None        # too short for ddmm


def test_parse_gga_rejects_when_lat_or_lon_unparseable():
    # Body has the right shape and a valid checksum, but the lat token
    # is too short to be ddmm.mmmm — must reject rather than emit (0,0).
    body = "$GNGGA,123519,4.7,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"
    s = 0
    for c in body.lstrip("$"):
        s ^= ord(c)
    line = f"{body}*{s:02X}"
    assert parse_gga(line) is None


# ───────────── Full GGA parsing ─────────────
def test_parse_gga_extracts_fix_quality_and_position():
    line = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
    out = parse_gga(line)
    assert out is not None
    assert out["quality"] == 1                     # standard GPS fix
    assert out["n_sat"] == 8
    assert out["lat"] == pytest.approx(48.1173, abs=1e-4)
    assert out["lon"] == pytest.approx(11.5167, abs=1e-3)
    assert out["alt"] == pytest.approx(545.4, abs=0.01)


def test_parse_gga_rejects_invalid_checksum():
    line = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*99"
    assert parse_gga(line) is None


def test_parse_gga_rejects_non_gga_sentence():
    line = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
    assert parse_gga(line) is None


def test_parse_gga_handles_gnss_combined_sentence():
    """$GNGGA is multi-constellation (GPS+GLONASS+Galileo)."""
    line = "$GNGGA,123519,4807.038,N,01131.000,E,4,12,0.6,545.4,M,46.9,M,,*58"
    out = parse_gga(line)
    assert out is not None
    assert out["quality"] == RTK_FIX_FIXED
    assert out["n_sat"] == 12


# ───────────── WGS84 → ENU ─────────────
def test_enu_at_reference_is_zero():
    """A point identical to the ENU reference must map to (0, 0, 0)."""
    enu = llh_to_enu(37.5665, 126.9780, 50.0,   # query == ref (Seoul)
                     37.5665, 126.9780, 50.0)
    assert abs(enu[0]) < 1e-3
    assert abs(enu[1]) < 1e-3
    assert abs(enu[2]) < 1e-3


def test_enu_north_displacement_matches_meters():
    """1 arc-minute of latitude ≈ 1849.8 m on the WGS84 ellipsoid
    (the often-cited 1852 m figure is a spherical-earth approximation;
    the ellipsoid value at this latitude is slightly smaller)."""
    enu = llh_to_enu(37.5665 + 1.0/60.0, 126.9780, 50.0,
                     37.5665, 126.9780, 50.0)
    # East ≈ 0
    assert abs(enu[0]) < 1.0
    # WGS84 north value at this latitude ≈ 1849.8 m (verified offline)
    assert enu[1] == pytest.approx(1850.0, abs=5.0)


def test_enu_east_displacement_at_seoul_latitude():
    """1 arc-minute of longitude at lat=37.57° on WGS84 ≈ 1472 m east.

    (Spherical estimate is cos(lat)·1852 ≈ 1468; ellipsoid is slightly
    larger because the prime vertical radius exceeds the mean radius.)
    """
    enu = llh_to_enu(37.5665, 126.9780 + 1.0/60.0, 50.0,
                     37.5665, 126.9780, 50.0)
    assert enu[0] == pytest.approx(1472.0, abs=5.0)
    assert abs(enu[1]) < 1.0


def test_enu_altitude_difference_appears_as_up():
    enu = llh_to_enu(37.5665, 126.9780, 100.0,
                     37.5665, 126.9780, 50.0)
    assert abs(enu[0]) < 0.5
    assert abs(enu[1]) < 0.5
    assert enu[2] == pytest.approx(50.0, abs=0.5)
