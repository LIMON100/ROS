# #!/usr/bin/env python3
# import rclpy
# from rclpy.node import Node
# from std_msgs.msg import Empty
# from geometry_msgs.msg import Twist
# import os
# import time

# def main():
#     rclpy.init()
#     node = rclpy.create_node('chaos_trigger')
    
#     # 1. Mute the Leader Heartbeat
#     pub_fail = node.create_publisher(Empty, '/simulate_fail', 10)
    
#     print(">>> CRASHING ROBOT-01...")
#     # Send multiple times to ensure it's received
#     msg = Empty()
#     for _ in range(10):
#         pub_fail.publish(msg)
#         time.sleep(0.05)
        
#     print(">>> ROBOT-01 HEARTBEAT STOPPED.")
#     print(">>> EXPECTED RESULT: Swarm will wait 3-4s, then Robot-02 will initiate 10s takeover.")

#     node.destroy_node()
#     rclpy.shutdown()

# if __name__ == '__main__':
#     main()


import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import os
import subprocess
import time

def main():
    rclpy.init()
    node = rclpy.create_node('chaos_trigger')
    pub_stop = node.create_publisher(Twist, '/cmd_vel', 10) 
    
    print(">>> TERMINATING GLOBAL (SH-01) NAVIGATION & MANAGEMENT...")
    
    # 1. HARD KILL: Kill by Executable Name, but exclude namespaced processes
    # We explicitly kill the leadership_manager to STOP HEARTBEATS
    target_execs = [
        'bt_navigator', 
        'planner_server', 
        'controller_server', 
        'leader_node', 
        'leadership_manager' # <--- This is the actual executable name
    ]
    
    for target in target_execs:
        # Find PIDs of these executables that are NOT in a namespace (Global R1)
        # We grep for the process, exclude grep itself, and exclude SH_ (followers)
        cmd = f"ps aux | grep '{target}' | grep -v 'SH_' | grep -v 'grep' | awk '{{print $2}}'"
        try:
            pids = subprocess.check_output(cmd, shell=True).decode().split()
            for pid in pids:
                print(f"Killing Global {target} (PID: {pid})")
                os.system(f"kill -9 {pid}")
        except:
            pass

    print(">>> STOPPING ROBOT-01 WHEELS...")
    stop_msg = Twist()
    for _ in range(5):
        pub_stop.publish(stop_msg)
        time.sleep(0.1)

    print(">>> REMOVING SH-01 PHYSICAL MODEL...")
    cmd = "gz service -s /world/obstacle_world/remove --reqtype gz.msgs.Entity --reptype gz.msgs.Boolean --timeout 2000 --req 'name: \"tin3_bot\", type: MODEL'"
    os.system(cmd)

    print(">>> R1 DESTROYED. Heartbeat should stop now.")
    node.destroy_node()
    rclpy.shutdown()