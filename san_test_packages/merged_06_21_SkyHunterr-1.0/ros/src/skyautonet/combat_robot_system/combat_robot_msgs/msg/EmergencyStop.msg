# SAN v1.3 IDS §3 - emergency halt command.
# Highest-priority message; bypasses autonomous behavior immediately.

uint8 SCOPE_SINGLE_ROBOT = 0
uint8 SCOPE_ALL_ROBOTS   = 1
uint8 SCOPE_LEADER_ONLY  = 2

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint8  scope                      # SCOPE_*
uint32 target_robot_id            # used only when scope == SCOPE_SINGLE_ROBOT
string reason                     # operator-facing text
string operator_id
uint64 timestamp_ms
