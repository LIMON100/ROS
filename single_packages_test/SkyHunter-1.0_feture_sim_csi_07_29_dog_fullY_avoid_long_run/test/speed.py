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

        # 왔다갔다 패턴: 가운데 → 왼쪽 → 가운데 → 오른쪽 → 가운데
        self.sequence = [0.0, -40.0, 0.0, 40.0, 0.0]

        # 속도/딜레이 좀 빠르게
        self.delay = 1.0   # 각 스텝 사이 대기 시간 (초)
        self.speed = 100    # pan_speed 값(원래 20이었음)

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
        self.get_logger().info("Start infinite pan step (Ctrl+C 로 종료하세요)")
        try:
            # 무한 반복
            while rclpy.ok():
                for angle in self.sequence:
                    self.publish_reliable(angle)
                    time.sleep(self.delay)
        except KeyboardInterrupt:
            self.get_logger().info("Stopped by user (KeyboardInterrupt)")


def main(args=None):
    rclpy.init(args=args)
    node = PanStepTest()
    node.run_sequence()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
