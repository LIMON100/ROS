# Command command_from
uint8 TABLET = 0x00

# Command ID
uint8 IDLE = 0
uint8 RECON = 1
uint8 PROTECT_GENERAL = 2
uint8 PROTECT_DRONE = 3
uint8 DEBUG_ATTACK = 4
uint8 DEBUG_TRACKING = 5
uint8 ASSAULT = 6
uint8 RETURN_TO_HOME = 7
uint8 ESTOP = 8

# Stream command
uint8 STREAM_NONE = 0
uint8 STREAM_START = 1
uint8 STREAM_STOP = 2

# UserCommand.msg
# This message is used to send user commands to the combat robot system.
std_msgs/Header header

uint8 command_from # 0 - tablet / 1 - ble

# Command ID
uint8 command_id # 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=Estop

# Manual Targeting Coordinates
float32 target_x # X coordinate in normalized pixels (0.0 - 1.0)
float32 target_y # Y coordinate in normalized pixels (0.0 - 1.0)

# Drone Search Target
float64 drone_target_lat
float64 drone_target_lon
bool    drone_target_valid

# Gun trigger control
bool gun_trigger # 0 - stop / 1 - start
bool gun_trigger_permission # 0 - no permission / 1 - permission granted

# Gimbal / stream control
int8 pan_speed
int8 tilt_speed
int8 zoom_command
uint8 stream_command # 0=None, 1=Start, 2=Stop
