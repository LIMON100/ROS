# SAN v1.5 IDS §5.2 — Breadcrumb broadcast.
#
# Each robot broadcasts its current + 1s-predicted pose at 10 Hz so
# neighbouring robots can compute Tier-2 close-proximity avoidance
# (SDD §6.2). This is a P0 reliable QoS short-range mesh broadcast.
#
# 권원:
#   * SDD-SWARM v1.5 §6.3 (1초 예측 broadcast)
#   * SDD-SWARM v1.5 §6.2 (T2 자율 회피)
#   * IDS-CMD v1.5 §5.2

std_msgs/Header header

uint32 sender_robot_id
uint32 sequence_number     # monotonic — for deduplication

# Current pose + velocity
geometry_msgs/Pose  current_pose
geometry_msgs/Twist current_velocity

# 1-second predicted pose (current pose + velocity * 1.0s)
geometry_msgs/Pose  pred_pose_1s

# Behavioural hints
uint8 ROLE_LEADER   = 0
uint8 ROLE_FOLLOWER = 1
uint8 ROLE_HUB      = 2
uint8 ROLE_DEPUTY   = 3
uint8 role

uint8 STATE_NORMAL    = 0
uint8 STATE_AVOIDING  = 1
uint8 STATE_RTB       = 2
uint8 STATE_LIMP      = 3
uint8 STATE_EMERGENCY = 4
uint8 state

# Optional fields populated when STATE_EMERGENCY
bool   emergency_stop_requested
string emergency_reason
