# SAN v1.3 IDS §3 - operator → swarm mission-state transition.

uint8 STATE_IDLE        = 0
uint8 STATE_PATROL      = 1
uint8 STATE_ENGAGE      = 2
uint8 STATE_RETURN      = 3
uint8 STATE_DOCK        = 4
uint8 STATE_HOLD        = 5

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint32 mission_id                 # operator-assigned mission identifier
uint8  target_state               # STATE_*
string detail                     # optional human-readable note
uint64 timestamp_ms
