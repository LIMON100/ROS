std_msgs/Header header

# Leader's current position and orientation (Global/Odom Frame)
geometry_msgs/Pose pose

# Leader's current velocity (Linear and Angular)
geometry_msgs/Twist velocity

# Formation Configuration
# 0 = V-Shape, 1 = Diamond, 2 = Column
uint8 formation_mode

# Formation Status
# 0 = NORMAL (Keep Formation)
# 1 = SAFETY_DETOUR (Break Formation / Avoid Obstacles)
# 2 = REJOIN (Return to Formation)
uint8 formation_state