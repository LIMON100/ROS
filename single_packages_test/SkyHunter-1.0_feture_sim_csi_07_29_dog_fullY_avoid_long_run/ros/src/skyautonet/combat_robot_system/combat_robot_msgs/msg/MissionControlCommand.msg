std_msgs/Header header

uint8 IDLE = 0
uint8 RECON = 1
uint8 PROTECT_GENERAL = 2
uint8 PROTECT_DRONE = 3
uint8 DEBUG_ATTACK = 4
uint8 DEBUG_TRACKING = 5
uint8 ASSAULT = 6
uint8 RETURN_TO_HOME = 7

uint8 ATTACK_PERMISSION_NONE = 0
uint8 ATTACK_PERMISSION_APPROVE = 1
uint8 ATTACK_PERMISSION_DENY = 2

uint8 command_id
bool estop_requested
uint8 attack_permission
int8 pan_speed
int8 tilt_speed
int8 zoom_command
float32 lateral_wind_speed
float64 drone_target_lat
float64 drone_target_lon
bool drone_target_valid
