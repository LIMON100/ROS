# SAN v1.5 IDS §4.5 — Follower tier transition event.
#
# Published by tier_node whenever the follower's tracking mode FSM
# transitions between T0-T4 states. Used as:
#   1. Operator UI alert for swarm health
#   2. KPP-2 evidence (T0→T1.5 transition timing ≤ 300ms)
#   3. Telemetry for post-mission analysis
#
# 권원:
#   * SDD-SWARM v1.5 §6.2 (5-Tier FSM)
#   * IDS-CMD v1.5 §4.5

std_msgs/Header header

uint32  robot_id

# Tier enum — match TierFsm internal codes.
# Note: T1.5 occupies position 2 between T1 and T2 because it was
# inserted in v1.5 (AUTO_REROUTE was added after the original 5 tiers).
uint8 TIER_T0    = 0    # PREDICTIVE_TRACK     — 1초 예측 정상 추종
uint8 TIER_T1    = 1    # NORMAL                — body-frame PID
uint8 TIER_T1_5  = 2    # AUTO_REROUTE ★ KPP-2  — ±2m 측방 우회
uint8 TIER_T2    = 3    # CATCH_UP              — 1.2x 가속
uint8 TIER_T3    = 4    # HARD_CATCH_UP         — 최대 속도
uint8 TIER_T4    = 5    # BREADCRUMB_RECOVERY   — 과거 경로 추적
uint8 previous_tier
uint8 current_tier

# Trigger reason — free-text for operator UI / log analysis.
# Standard values:
#   "prediction_received"      — entering T0
#   "prediction_lost"          — entering T1
#   "obstacle_on_path"         — entering T1.5 ★
#   "obstacle_cleared"         — leaving T1.5
#   "delta_gt_1.5_d0"          — entering T2
#   "delta_gt_2.0_d0"          — entering T3
#   "delta_gt_4.0_d0"          — entering T4
#   "comm_timeout_60s"         — entering T4
#   "comm_restored"            — leaving T4
string reason

# Measurement evidence — populated for telemetry/replay.
float32 delta_m              # current distance from target slot (m)
float32 base_distance_d0_m   # formation spacing preset
bool    obstacle_detected
uint32  prediction_loss_ms   # accumulated time since last prediction

uint64  timestamp_ms
