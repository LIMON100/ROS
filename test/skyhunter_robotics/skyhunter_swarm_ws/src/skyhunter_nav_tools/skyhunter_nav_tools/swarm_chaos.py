#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Empty
import subprocess
import time

def main():
    rclpy.init()
    node = rclpy.create_node('chaos_trigger')
    pub = node.create_publisher(Empty, '/simulate_fail', 10)
    
    print(">>> SENDING MUTE SIGNAL TO LEADER NODE...")
    msg = Empty()
    pub.publish(msg)
    
    # Give it a moment to ensure the message is sent over DDS
    time.sleep(1.0)

    print(">>> KILLING GLOBAL NAV2 (ROBOT-1) ONLY...")
    # These commands kill nodes in the global namespace (/) 
    # but WILL NOT touch nodes in /SH_02/
    subprocess.run(["pkill", "-f", "^/bt_navigator"])
    subprocess.run(["pkill", "-f", "^/controller_server"])
    subprocess.run(["pkill", "-f", "^/planner_server"])

    print("Robot-1 is physically and logically dead.")
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()