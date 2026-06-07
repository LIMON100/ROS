# SAN v1.3 IDS §3 - operator manual control override.
# Bypasses autonomous behavior for a single robot.

uint8 OVERRIDE_CMD_VEL    = 0    # direct linear/angular velocity
uint8 OVERRIDE_HALT       = 1    # immediate stop, hold ground
uint8 OVERRIDE_RETURN     = 2    # return to base
uint8 OVERRIDE_RELEASE    = 3    # release back to autonomy

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint32 target_robot_id
uint8  override_type              # OVERRIDE_*
geometry_msgs/Twist cmd_vel       # used only when override_type == CMD_VEL
uint32 max_duration_sec           # auto-release after this
string operator_id
uint64 timestamp_ms
