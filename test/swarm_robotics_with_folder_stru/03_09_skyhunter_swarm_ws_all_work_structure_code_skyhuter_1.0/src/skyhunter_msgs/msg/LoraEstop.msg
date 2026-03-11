# Emergency stop command via LoRa
std_msgs/Header header
uint8 command_type             # 1=estop, 2=resume, 3=RTB
uint8 target_robot             # 0xFF = broadcast to all
uint16 auth_token
uint16 seq_number
