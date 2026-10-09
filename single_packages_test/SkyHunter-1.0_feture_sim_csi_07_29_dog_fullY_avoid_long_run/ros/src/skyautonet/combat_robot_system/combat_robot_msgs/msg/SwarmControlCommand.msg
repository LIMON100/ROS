std_msgs/Header header

uint8 FORMATION_NONE = 0
uint8 FORMATION_RECON = 1
uint8 FORMATION_PROTECT = 2
uint8 FORMATION_ASSAULT = 3

uint8 formation_type
uint8 formation_number
uint8 grouping_index
uint8 selected_robot_count
uint32[8] selected_robot_ids
