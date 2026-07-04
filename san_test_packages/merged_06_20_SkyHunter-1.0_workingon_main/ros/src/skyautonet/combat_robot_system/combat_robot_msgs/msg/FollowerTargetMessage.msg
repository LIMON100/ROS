# SAN v1.5 IDS §5.1 — Follower target message (KPP-1, KPP-3 핵심).
#
# Leader broadcasts target waypoints with 1-second prediction so
# followers can pre-position. Published at 10 Hz (P0 QoS, reliable).
#
# 권원:
#   * SDD-SWARM v1.5 §6.3 (1초 예측 broadcast)
#   * IDS-CMD v1.5 §5.1
#   * KPP-1 (대열 유지 ≤ 2m)
#   * KPP-3 (통신 지연 ≤ 150ms)

std_msgs/Header header

uint32  target_robot_id    # which follower this message is for
uint32  formation_epoch    # must match the latest SlotAssignment

# Current target — where follower should move to RIGHT NOW
geometry_msgs/Pose target_pose_now

# 1-second predicted target — where follower SHOULD BE in 1.0s
# Used for pre-positioning to absorb communication latency.
geometry_msgs/Pose target_pose_pred_1s

# Velocity command (leader's frame). Followers track this to stay
# in formation during leader's manoeuvres.
geometry_msgs/Twist target_velocity

# Maximum allowed speed for this follower (m/s). Respects operational
# mode (RECON=1.3, COMBAT=2.0, RTB=1.5, DEV_TEST=1.0).
float32 max_speed_mps

# Lead Bias — how much the follower should aggressively track ahead
# of the predicted pose to compensate for control-loop latency.
# Typical: 0.0..0.3 seconds extra lookahead.
float32 lead_bias_s
