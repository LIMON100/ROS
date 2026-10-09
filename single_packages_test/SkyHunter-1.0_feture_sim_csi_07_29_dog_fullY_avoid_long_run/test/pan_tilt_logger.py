#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import csv
from datetime import datetime
import yaml
import os

from combat_robot_msgs.msg import PanTiltControlCommand, PanTiltState


class PanTiltLogger(Node):
    def __init__(self):
        super().__init__("pan_tilt_logger")

        # --- YAML 파일 경로 ---
        yaml_path = "/home/firefly/combatrobot_1/ros/src/skyautonet/combat_robot_system/pan_tilt_controller/param/pan_tilt.param.yaml"

        # --- YAML 파일 로드 ---
        if os.path.exists(yaml_path):
            with open(yaml_path, 'r') as f:
                data = yaml.safe_load(f)

            params = data["/**"]["ros__parameters"]

            self.pan_gain = float(params.get("pan_proportional_gain", 0.0))
            self.tilt_gain = float(params.get("tilt_proportional_gain", 0.0))
        else:
            self.pan_gain = 0.0
            self.tilt_gain = 0.0

        self.get_logger().info(f"Loaded YAML gains → pan_gain={self.pan_gain}, tilt_gain={self.tilt_gain}")

        # --- CSV 파일 생성 ---
        now = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.csv_path = f"/home/firefly/combatrobot_1/ros/pan_tilt_log_{now}.csv"

        with open(self.csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "timestamp(sec)",
                "target_pan", "target_tilt",
                "current_pan", "current_tilt",
                "pan_error", "tilt_error",
                "final_pan_speed", "final_tilt_speed",
                "pan_gain", "tilt_gain"
            ])

        self.get_logger().info(f"CSV 로깅 시작 → {self.csv_path}")

        # 최신 명령 저장용 변수
        self.latest_cmd = PanTiltControlCommand()

        # --- Subscribers ---
        self.create_subscription(
            PanTiltControlCommand,
            "/pan_tilt_control_command",
            self.cmd_callback,
            10
        )

        self.create_subscription(
            PanTiltState,
            "/current_actuator_state_info",
            self.state_callback,
            10
        )

    def cmd_callback(self, msg: PanTiltControlCommand):
        self.latest_cmd = msg

    def state_callback(self, msg: PanTiltState):
        timestamp = msg.stamp.sec + msg.stamp.nanosec * 1e-9

        with open(self.csv_path, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                timestamp,
                self.latest_cmd.horizontal_angle,
                self.latest_cmd.vertical_angle,
                msg.horizontal_angle,
                msg.vertical_angle,
                msg.pan_error,
                msg.tilt_error,
                msg.final_pan_speed,
                msg.final_tilt_speed,
                self.pan_gain,
                self.tilt_gain
            ])


def main(args=None):
    rclpy.init()
    node = PanTiltLogger()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
