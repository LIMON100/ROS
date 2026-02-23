#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Empty
import sys

def main():
    rclpy.init()
    node = rclpy.create_node('chaos_trigger')
    # Trigger fail for a specific robot (default SH_01)
    target = sys.argv[1] if len(sys.argv) > 1 else "SH_01"
    pub = node.create_publisher(Empty, f'/{target}/simulate_fail', 10)
    
    print(f"SIMULATING FAILURE FOR {target}...")
    pub.publish(Empty())
    
    node.destroy_node()
    rclpy.shutdown()