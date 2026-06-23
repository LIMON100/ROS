std_msgs/Header header
uint32  shooter_robot_id
bool    solution_valid
float32 aim_bearing_deg      # world-frame lead bearing (where to aim)
float32 aim_elevation_deg
float32 range_m
float32 lead_time_s
float32 target_rate_dps      # estimated angular speed of target (deg/s)
uint8   vote_count           # peers confirming this target
bool    engage_ready         # solution_valid && vote_count >= min_votes
uint64  timestamp_ms