import sys
if sys.prefix == '/usr':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/limonubuntu/Work/Limon/ros2_work/swarm_robots/skyhunter_swarm_ws/install/skyhunter_nav_tools'
