# SAN v1.3 IDS §3 - operator → robot multi-waypoint route.

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint32 target_robot_id
geometry_msgs/Point[] waypoints   # WGS84 lat/lon as Point.x/.y, alt as .z
float32 cruise_speed_mps
bool   loop_when_done
uint64 timestamp_ms
