# combat_robot_msgs/msg/DriveCommand.msg
std_msgs/Header header
float32 linear_velocity    # m/s, forward(+)/backward(-)
float32 angular_velocity   # rad/s, left(+)/right(-)  ← 기존 부호 규칙 그대로 사용
