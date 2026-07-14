# SAN v1.3 IDS §3 - operator → swarm formation change command.

uint8 FORMATION_COLUMN  = 0
uint8 FORMATION_LINE    = 1
uint8 FORMATION_WEDGE   = 2
uint8 FORMATION_DIAMOND = 3
uint8 FORMATION_CUSTOM  = 4

std_msgs/Header header
uint32 command_id                 # PHASE 7: echoed by RobotStatus.last_received_command_id
uint32 sequence
uint32 leader_robot_id
uint8  formation                  # FORMATION_*
float32 target_spacing_m          # nominal inter-robot distance (d0)
float32 leader_heading_deg        # initial heading at formation entry
uint64 timestamp_ms
