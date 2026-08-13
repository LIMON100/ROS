#!/usr/bin/env python3
# 팔로워 board 토픽 브리지: /sN/swarm/path_command → /sN/mission/path_command
# (board2 executor가 mission/path_command 구독하는 버그 우회, 정식 command_server fan-out 유지)
import sys, rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
from combat_robot_msgs.msg import SwarmPathCommand
ns = sys.argv[1] if len(sys.argv) > 1 else "s2"
rclpy.init(); n = Node("path_relay")
q = QoSProfile(depth=10); q.reliability = ReliabilityPolicy.RELIABLE
q.history = HistoryPolicy.KEEP_LAST; q.durability = DurabilityPolicy.VOLATILE
pub = n.create_publisher(SwarmPathCommand, f"/{ns}/mission/path_command", q)
def cb(m):
    pub.publish(m); n.get_logger().info(f"relay cmd={m.command} nw={m.num_waypoints}")
n.create_subscription(SwarmPathCommand, f"/{ns}/swarm/path_command", cb, q)
n.get_logger().info(f"relay /{ns}/swarm/path_command -> /{ns}/mission/path_command")
rclpy.spin(n)
