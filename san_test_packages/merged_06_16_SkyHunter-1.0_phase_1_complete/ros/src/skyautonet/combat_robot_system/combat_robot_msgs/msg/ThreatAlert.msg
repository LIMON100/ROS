# SAN v1.5 IDS — Threat alert message (Phase 2-E Turn 8).
#
# Published by per-robot detectors on /{robot_id}/threat_alert AND
# by hub_orchestrator aggregator on /hub/threat_alert (after dedup +
# severity escalation + time-window aggregation).
#
# Bidirectional flow:
#   robot_N → san_threat_aggregator → hub aggregated → operator UI
#   robot_N → san_threat_aggregator → swarm peers (degraded-mode routing)
#
# 권원:
#   * SDD-SWARM v1.5 §3.4.2 (Hub-level threat aggregation)
#   * IDS v1.5 §5.18 (ThreatAlert topic naming)

# Severity levels (uint8 — wire-format efficient)
uint8 SEVERITY_INFO     = 0
uint8 SEVERITY_WARNING  = 1
uint8 SEVERITY_CRITICAL = 2
uint8 SEVERITY_FATAL    = 3

# Threat types
uint8 TYPE_SBC_FAILED         = 1    # peer SBC silent past timeout
uint8 TYPE_BATTERY_CRITICAL   = 2    # battery <20%
uint8 TYPE_RTK_LOST           = 3    # GNSS RTK fix degraded
uint8 TYPE_LTE_OUTAGE         = 4    # primary cellular link down
uint8 TYPE_DRONE_DETECTED     = 5    # hostile UAV in RF/optical
uint8 TYPE_OBSTACLE_BLOCKED   = 6    # path blocked, no recovery
uint8 TYPE_FIRE_AUTH_DENIED   = 7    # weapon authorization failed
uint8 TYPE_COMM_LINK_DEGRADED = 8    # WiFi6+LTE both flaky
uint8 TYPE_FOLLOWER_LOST      = 9    # follower out of mesh range
uint8 TYPE_OTHER              = 99

std_msgs/Header header

uint8  severity                  # SEVERITY_*
uint8  threat_type               # TYPE_*

string source_robot_id           # which robot reported (or "hub")
string peer_id                   # affected peer/target (may be empty)

string message_ko                # operator-facing Korean text
string detail                    # machine-readable (JSON-encoded)

uint64 timestamp_ms              # event time (epoch ms)
uint32 instance_count            # # of underlying events folded into this alert

# Spatial localization of the threat — used by san_surveillance to steer
# sectors / pan-tilt at the threat instead of the front. Reporters that
# don't have a position (e.g. SBC_FAILED, BATTERY_CRITICAL) leave these
# at zero — consumers check `has_position` before using bearing/elevation.
bool    has_position
float32 bearing_deg              # world-frame bearing, 0 = +x, CCW positive
float32 elevation_deg            # horizon-frame elevation, 0 = horizon
float32 range_m                  # 0 if unknown
