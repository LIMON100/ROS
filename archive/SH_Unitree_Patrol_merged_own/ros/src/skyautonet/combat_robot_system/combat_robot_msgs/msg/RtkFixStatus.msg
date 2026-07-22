# SAN v1.5 IDS — RTK fix detailed status (Phase 2-E Turn 4).
#
# Complements sensor_msgs/NavSatFix with RTK-specific quality info
# that downstream consumers (SLAM, mission planner, safety) need:
#   - fix_type (NO_FIX / 2D / 3D / DGPS / RTK_FLOAT / RTK_FIX)
#   - num_satellites_view, num_satellites_used
#   - HDOP/VDOP/PDOP
#   - RTK age (s) — distance from last RTCM correction
#
# 권원:
#   * SDD-SWARM v1.5 §3.1.2 (RTK GNSS HW — u-blox F9P)
#   * IDS v1.5 §5 (RTK topic naming)
#   * NMEA 0183 §5.3.20 (GGA fix_quality field)

# fix_type — derived from NMEA $GxGGA field 6
uint8 FIX_NO        = 0
uint8 FIX_AUTO_2D   = 1
uint8 FIX_DGPS      = 2
uint8 FIX_PPS       = 3
uint8 FIX_RTK_FIX   = 4   # RTK 정밀 — 1 cm 정확도
uint8 FIX_RTK_FLOAT = 5   # RTK float — 10 cm 정확도
uint8 FIX_ESTIMATED = 6   # Dead reckoning
uint8 FIX_MANUAL    = 7
uint8 FIX_SIMULATED = 8

std_msgs/Header header

uint8  fix_type                # FIX_*
uint8  num_satellites_view     # 0..40
uint8  num_satellites_used     # 0..40 (used in current fix)

# Coordinates (decimal degrees, signed)
float64 latitude_deg
float64 longitude_deg
float64 altitude_m              # MSL altitude
float64 geoid_separation_m      # WGS84 - MSL

# Quality
float32 hdop                    # horizontal dilution of precision
float32 vdop                    # vertical dilution
float32 pdop                    # position 3D dilution

# RTK aging — seconds since last RTCM correction was injected.
# >10s ⇒ correction stale, fix may downgrade to AUTO_2D/DGPS.
float32 rtk_correction_age_s    # NaN = never received

# Reference station ID (NMEA GGA field 14). 0 = none.
uint16 reference_station_id

# Statistics
uint32 dropped_nmea_count       # checksum failures + parse errors
uint32 rtcm_inject_count        # RTCM frames written to receiver
uint64 last_fix_timestamp_ms
