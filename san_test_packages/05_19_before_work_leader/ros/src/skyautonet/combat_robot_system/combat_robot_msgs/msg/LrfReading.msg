# SAN v1.5 IDS — Single-point laser range finder reading (Phase 2-E Turn 7).
#
# Published by san_lidar::LrfNode at ~5 Hz. Used for narrow-beam range
# queries (obstacle clearance check before turn) that the scanning
# LiDAR can't resolve at distance.
#
# 권원:
#   * SDD-SWARM v1.5 §3.1.3 (LRF HW — Lightware LW20 등)
#   * IDS v1.5 §5 (LRF topic naming)

std_msgs/Header header

float32 range_m                # 0 = no return / out of range
float32 return_strength        # 0..1 (manufacturer specific)
bool    valid                  # true iff range is trustworthy

# Statistics
uint32  dropped_count          # parse errors / read failures (cumulative)
uint64  last_read_timestamp_ms
