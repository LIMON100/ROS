std_msgs/Header header
geometry_msgs/Pose pose              
geometry_msgs/Twist velocity         
geometry_msgs/Pose[] next_waypoints
uint8 formation_mode
uint8 formation_state
int8 swarm_state  # 0: NAVIGATING, 1: GOAL_REACHED, 2: TRANSITIONING
int8 formation_type  # 0=V-Shape, 1=Column, 2=Diamond
int32 current_waypoint_index