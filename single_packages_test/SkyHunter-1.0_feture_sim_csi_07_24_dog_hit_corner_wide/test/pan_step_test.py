#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import time
from combat_robot_msgs.msg import PanTiltControlCommand

class PanStepTest(Node):
    def __init__(self):
        super().__init__('pan_step_test_node')

        self.pub = self.create_publisher(
            PanTiltControlCommand,
            '/pan_tilt_control_command',
            10
        )

        self.sequence = [0.0, -5.0, 0.0, 5.0, 0.0]
        self.delay = 6.0
        self.speed = 20

    def publish_reliable(self, angle):
        """6Hz ~ 50Hz 환경에서도 절대 씹히지 않도록 0.6초 반복 publish"""
        start = time.time()
        while time.time() - start < 0.6:   # 0.6초 동안 전달
            msg = PanTiltControlCommand()
            msg.control_mode = 1
            msg.horizontal_angle = float(angle)
            msg.vertical_angle = 0.0
            msg.pan_speed = self.speed
            msg.tilt_speed = 0
            msg.pan_dir = 0
            msg.tilt_dir = 0

            self.pub.publish(msg)
            time.sleep(0.02)  # 20ms → 50Hz publish

        self.get_logger().info(f"[SEND CONFIRMED] angle={angle}, speed={self.speed}")


    def run_sequence(self):
        for angle in self.sequence:
            self.publish_reliable(angle)
            time.sleep(self.delay)


def main(args=None):
    rclpy.init(args=args)
    node = PanStepTest()
    node.run_sequence()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
