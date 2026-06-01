# SAN v1.5 IDS — operator → swarm formation change command.
#
# IDs align with FormationStatus.msg (1..9, SDD-SWARM §7).
# Pre-PDR v1.3 used 0..4; the v1.3 set is no longer accepted.

uint8 FORMATION_COLUMN         = 1
uint8 FORMATION_LINE           = 2
uint8 FORMATION_V_SHAPE        = 3
uint8 FORMATION_DIAMOND        = 4
uint8 FORMATION_ECHELON_LEFT   = 5
uint8 FORMATION_ECHELON_RIGHT  = 6
uint8 FORMATION_BOX            = 7
uint8 FORMATION_VEE_INVERTED   = 8
uint8 FORMATION_FREE_SPREAD    = 9

std_msgs/Header header
uint32 command_id                 # PHASE 7: echoed by RobotStatus.last_received_command_id
uint32 sequence
uint32 leader_robot_id
uint8  formation                  # FORMATION_* (1..9)
float32 target_spacing_m          # nominal inter-robot distance (d0)
float32 leader_heading_deg        # initial heading at formation entry
uint64 timestamp_ms
