# SAN v1.5 IDS §5.9 — Slot assignment from Hungarian algorithm.
#
# Published by formation_node after solving the assignment problem
# (current robot poses → target formation slots). Each follower reads
# its assigned slot_index and the corresponding world position.
#
# 권원:
#   * SDD-SWARM v1.5 §7.4 (Hungarian alg.)
#   * IDS-CMD v1.5 §5.9 (메시지 본문)

std_msgs/Header header

# Assignment epoch — increments each time the formation re-plans.
# Followers ignore older epochs to avoid race conditions during transitions.
uint32  epoch

# Formation identifier — see Formations enum (1..9).
#   1=Column 2=Line 3=VShape 4=Diamond 5=EchelonLeft
#   6=EchelonRight 7=Box 8=VeeInverted 9=FreeSpread
uint8   formation_id

# Preset spacing/angle that produced these slots.
float32 spacing_d         # metres
float32 spread_theta_deg  # degrees (V/Vee-Inverted only)

# Per-robot assignment. Parallel arrays — robot_ids[i] is assigned
# to world position (slot_x_m[i], slot_y_m[i]).
uint32[]  robot_ids
float32[] slot_x_m        # world-frame target X
float32[] slot_y_m        # world-frame target Y
uint8[]   slot_index      # 0..N-1 within the formation pattern

# Total optimal cost of this assignment (sum of distance moves).
float32   total_cost_m
