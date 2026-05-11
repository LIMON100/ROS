#!/usr/bin/env python3
"""
Test script to send cmd_vel to multiple robots.

Usage:
  Terminal 1 - Spawn robots along X:
    ros2 launch tin3_gz_simulation sim_launch.py num_robots:=4 pattern:=line_x

  Terminal 2 - Run this script:
    python3 test_multi_robot.py

  Or with custom velocity:
    python3 test_multi_robot.py --linear 0.5 --angular 0.1
"""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import argparse


class MultiRobotController(Node):
    def __init__(self, num_robots=4, linear_vel=3.0, angular_vel=0.0):
        super().__init__('multi_robot_controller')

        self.num_robots = num_robots
        self.linear_vel = linear_vel
        self.angular_vel = angular_vel

        # Create publishers for each robot
        self.cmd_vel_pubs = []
        for i in range(1, num_robots + 1):
            topic = f'/robot_{i:02d}/cmd_vel'
            pub = self.create_publisher(Twist, topic, 10)
            self.cmd_vel_pubs.append(pub)
            self.get_logger().info(f'Publishing to: {topic}')

        # Timer to publish commands
        self.timer = self.create_timer(0.1, self.publish_cmd_vel)
        self.get_logger().info(f'Sending linear={linear_vel}, angular={angular_vel} to {num_robots} robots')

    def publish_cmd_vel(self):
        msg = Twist()
        msg.linear.x = self.linear_vel
        msg.angular.z = self.angular_vel

        for pub in self.cmd_vel_pubs:
            pub.publish(msg)


def main():
    parser = argparse.ArgumentParser(description='Control multiple robots')
    parser.add_argument('--num_robots', type=int, default=4, help='Number of robots')
    parser.add_argument('--linear', type=float, default=3.0, help='Linear velocity (m/s)')
    parser.add_argument('--angular', type=float, default=0.0, help='Angular velocity (rad/s)')
    args = parser.parse_args()

    rclpy.init()
    node = MultiRobotController(
        num_robots=args.num_robots,
        linear_vel=args.linear,
        angular_vel=args.angular
    )

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        # Stop robots before exiting
        stop_msg = Twist()
        for pub in node.cmd_vel_pubs:
            pub.publish(stop_msg)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()