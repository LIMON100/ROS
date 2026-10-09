#!/usr/bin/env python3
# scripts/keyboard_teleop.py

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import sys
import select
import termios
import tty
import time

class KeyboardTeleop(Node):
    def __init__(self):
        super().__init__('keyboard_teleop')
        self.publisher_ = self.create_publisher(Twist, '/user_command', 1)

        # 속도 제한
        self.max_linear = 100.0  # Internal max linear velocity (scaled to 2.22 m/s)
        self.max_angular = 100.0  # Internal max angular velocity (scaled to 6.0 rad/s)

        # 가속 관련 파라미터
        self.linear_increment = 1.0   # Internal linear increment
        self.angular_increment = 1.0   # Internal angular increment

        # 현재 속도 상태
        self.linear_velocity = 0.0
        self.angular_velocity = 0.0

        self.last_key = None
        self.last_time = time.time()

        self.get_logger().info("====Keyboard Teleop Control====")
        self.get_logger().info("W/S for Linear")
        self.get_logger().info("A/D for Angular")
        self.get_logger().info("Q for Stop.")
        self.get_logger().info("Internal velocity range: +-100")
        self.get_logger().info("scaled to Linear +-2.22 m/s")
        self.get_logger().info("Angular +-6 rad/s. ")
        self.get_logger().info("Ctrl+C to exit.")
        self.get_logger().info("================================")



    def get_key(self):
        tty.setraw(sys.stdin.fileno())
        rlist, _, _ = select.select([sys.stdin], [], [], 0.05)
        key = sys.stdin.read(1) if rlist else ''
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
        return key

    def update_velocity(self, key):
        # 선속도
        if key == 'w':
            self.linear_velocity += self.linear_increment
            self.linear_velocity = min(self.linear_velocity, self.max_linear)
        elif key == 's':
            self.linear_velocity -= self.linear_increment
            self.linear_velocity = max(self.linear_velocity, -self.max_linear)
        else:
           self.linear_velocity = 0.0

        # 각속도
        if key == 'a':
            self.angular_velocity += self.angular_increment
            self.angular_velocity = min(self.angular_velocity, self.max_angular)
        elif key == 'd':
            self.angular_velocity -= self.angular_increment
            self.angular_velocity = max(self.angular_velocity, -self.max_angular)
        else:
            self.angular_velocity = 0.0

        # 정지 키
        if key == 'q':
            self.linear_velocity = 0.0
            self.angular_velocity = 0.0

    def run(self):
        try:
            while rclpy.ok():
                key = self.get_key()
                self.update_velocity(key)

                twist = Twist()
                # Scale internal velocities to desired output range
                scaled_linear_velocity = self.linear_velocity * (2.22 / 100.0)
                scaled_angular_velocity = self.angular_velocity * (6.0 / 100.0)

                twist.linear.x = scaled_linear_velocity
                twist.angular.z = scaled_angular_velocity
                self.publisher_.publish(twist)

                if key:
                    self.get_logger().info(
                        f"linear: {self.linear_velocity:.2f} (internal), angular: {self.angular_velocity:.2f} (internal)"
                    )

                rclpy.spin_once(self, timeout_sec=0.05)
        except KeyboardInterrupt:
            pass

def main(args=None):
    global settings
    settings = termios.tcgetattr(sys.stdin)
    rclpy.init(args=args)
    node = KeyboardTeleop()
    node.run()
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()