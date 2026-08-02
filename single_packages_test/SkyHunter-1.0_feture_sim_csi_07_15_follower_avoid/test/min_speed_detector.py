#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import time

from combat_robot_msgs.msg import PanTiltControlCommand, PanTiltState

class ManualSpeedTest(Node):
    def __init__(self):
        super().__init__('manual_speed_test')

        # Declare user parameters
        self.axis = self.declare_parameter("axis", "PAN").value   # PAN or TILT
        self.speed = self.declare_parameter("speed", 1).value     # 테스트할 speed 값
        self.test_direction = self.declare_parameter("test_direction", 1).value  # 1 또는 2
        self.duration = self.declare_parameter("duration", 2.0).value            # 관찰 시간(초)

        self.get_logger().info("===== Manual PTZ Speed Test =====")
        self.get_logger().info(f"axis={self.axis}, speed={self.speed}, direction={self.test_direction}")

        # 상태 구독
        self.state_sub = self.create_subscription(
            PanTiltState,
            '/current_actuator_state_info',
            self.state_callback,
            10
        )

        # 명령 발행
        self.cmd_pub = self.create_publisher(
            PanTiltControlCommand,
            '/pan_tilt_control_command',
            10
        )

        self.latest_pan = None
        self.latest_tilt = None

        # 테스트 시작
        self.create_timer(1.0, self.run_test)

    def state_callback(self, msg):
        self.latest_pan = msg.horizontal_angle
        self.latest_tilt = msg.vertical_angle

    def send_dir(self, speed):
        cmd = PanTiltControlCommand()
        cmd.control_mode = PanTiltControlCommand.CONTROL_DIR

        if self.axis == "TILT":
            cmd.horizontal_angle = 9999.0
            cmd.vertical_angle = 0.0
            cmd.pan_speed = speed
            cmd.tilt_speed = 0
        else:
            cmd.horizontal_angle = 0.0
            cmd.vertical_angle = 9999.0
            cmd.pan_speed = 0
            cmd.tilt_speed = speed

        self.cmd_pub.publish(cmd)

    def run_test(self):
        if self.latest_pan is None or self.latest_tilt is None:
            return  # 아직 상태 못받음

        initial = self.latest_pan if self.axis == "PAN" else self.latest_tilt

        self.get_logger().info(f"초기 각도 = {initial:.3f}°")
        self.get_logger().info(f"Speed {self.speed} 명령 전송")

        # 명령 보내기
        self.send_dir(self.speed)

        # 관찰
        start = time.time()
        while time.time() - start < self.duration:
            current = self.latest_pan if self.axis == "PAN" else self.latest_tilt
            diff = current - initial
            self.get_logger().info(f"현재={current:.3f}° (Δ={diff:.3f})")
            time.sleep(0.3)

        self.get_logger().info("===== 테스트 종료 =====")
        rclpy.shutdown()


def main(args=None):
    rclpy.init(args=args)
    node = ManualSpeedTest()
    rclpy.spin(node)


if __name__ == '__main__':
    main()
