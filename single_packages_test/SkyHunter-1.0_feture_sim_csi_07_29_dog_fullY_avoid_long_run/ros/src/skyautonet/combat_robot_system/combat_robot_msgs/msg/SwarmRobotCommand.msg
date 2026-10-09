std_msgs/Header header

uint8 COMMAND_NONE = 0
uint8 COMMAND_MODE = 1
uint8 COMMAND_PATH = 2
uint8 COMMAND_FORMATION = 3
uint8 COMMAND_SYNC = 4

uint8 PATH_CMD_NONE = 0
uint8 PATH_CMD_START = 1
uint8 PATH_CMD_STOP = 2
uint8 PATH_CMD_PAUSE = 3
uint8 PATH_CMD_RESUME = 4
uint8 PATH_CMD_LOAD_PATH = 5

uint32 sequence
uint8 command_type
uint32 leader_robot_id
uint32 target_robot_id

uint8 operation_mode
bool estop_requested

uint8 path_command
uint16 num_waypoints
string path_id
string path_json

uint8 formation_type
uint8 formation_number
uint8 grouping_index
uint8 slot_index
uint8 selected_robot_count
uint32[8] selected_robot_ids
