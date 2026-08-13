std_msgs/Header header

uint8 LINK_DISCONNECTED = 0
uint8 LINK_CONNECTED = 1

uint32 robot_id
uint32 leader_robot_id
uint8 link_status
uint32 last_heartbeat_sequence
float32 heartbeat_age_sec

uint8 last_operation_mode
uint8 last_formation_type
uint8 last_formation_number
uint8 last_grouping_index

# Follower position so the operator-host (leader) command_server can place this
# robot on the tablet swarm map. Filled by the follower's own command_server from
# its NavSatFix (/sN/fix); 0 until a fix is received.
float64 latitude
float64 longitude
float32 heading_deg
float32 ground_speed_mps
