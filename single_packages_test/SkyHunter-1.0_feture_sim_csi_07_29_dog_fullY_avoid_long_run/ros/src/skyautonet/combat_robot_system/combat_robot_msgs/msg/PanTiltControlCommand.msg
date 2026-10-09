uint8 CONTROL_BRAKE = 0
uint8 CONTROL_HOR_POS = 1
uint8 CONTROL_VER_POS = 2
uint8 CONTROL_DIR = 3

# Pan-Tilt Control Command message
std_msgs/Header header
uint8 control_mode
float32 horizontal_angle
float32 vertical_angle
uint8 pan_speed
uint8 tilt_speed
uint8 pan_dir  # 0 - stop / 1 - right / 2 - left
uint8 tilt_dir # 0 - stop / 1 - up / 2 - down