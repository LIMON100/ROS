#!/usr/bin/env python3
# 사용법: swarm_cmd.py <topic> <command:load|start|stop> [path_json]
# LOAD: path_json 필수. START/STOP: path_json 무시.
import sys, rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
from combat_robot_msgs.msg import SwarmPathCommand

CMD = {"start": 1, "stop": 2, "pause": 3, "resume": 4, "load": 5, "complete": 6}

def main():
    topic = sys.argv[1]
    cmd = CMD[sys.argv[2].lower()]
    pj = sys.argv[3] if len(sys.argv) > 3 else ""
    # waypoint 수 세기(대략): '{' 개수 or '[' 좌표쌍
    nw = pj.count("{") if "{" in pj else max(0, pj.count("[") - 1)
    rclpy.init()
    n = Node("swarm_cmd_pub")
    q = QoSProfile(depth=10)
    q.reliability = ReliabilityPolicy.RELIABLE
    q.history = HistoryPolicy.KEEP_LAST
    q.durability = DurabilityPolicy.TRANSIENT_LOCAL  # late-join 구독자도 받도록
    pub = n.create_publisher(SwarmPathCommand, topic, q)
    m = SwarmPathCommand()
    m.header.stamp = n.get_clock().now().to_msg()
    m.command = cmd
    m.num_waypoints = nw
    m.path_json = pj
    import time
    # 구독자 발견 대기(discovery race 방지)
    for _ in range(50):
        if pub.get_subscription_count() > 0:
            break
        rclpy.spin_once(n, timeout_sec=0.1)
        time.sleep(0.1)
    subs = pub.get_subscription_count()
    for _ in range(8):
        pub.publish(m)
        rclpy.spin_once(n, timeout_sec=0.1)
        time.sleep(0.15)
    n.get_logger().info(f"published cmd={cmd} nw={nw} to {topic} (subs={subs})")
    time.sleep(0.5)
    n.destroy_node(); rclpy.shutdown()

if __name__ == "__main__":
    main()
