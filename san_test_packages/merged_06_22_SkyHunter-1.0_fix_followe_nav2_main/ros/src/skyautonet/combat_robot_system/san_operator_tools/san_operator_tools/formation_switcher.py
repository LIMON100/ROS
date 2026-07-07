# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Formation switcher operator tool.

Adapted from skyhunter_nav_tools::formation_switcher (Limon code).
Publishes a single FormationCommand to change the current swarm
formation. Maps to combat_robot_msgs/FormationCommand enums (v1.5
IDs 1..9 — matches FormationStatus.msg):
    column          → FORMATION_COLUMN         (1)
    line            → FORMATION_LINE           (2)
    vshape / wedge  → FORMATION_V_SHAPE        (3)
    diamond         → FORMATION_DIAMOND        (4)
    echelon_left    → FORMATION_ECHELON_LEFT   (5)
    echelon_right   → FORMATION_ECHELON_RIGHT  (6)
    box             → FORMATION_BOX            (7)
    vee_inverted    → FORMATION_VEE_INVERTED   (8)
    free_spread     → FORMATION_FREE_SPREAD    (9)

Usage:
    ros2 run san_operator_tools formation_switcher wedge
    ros2 run san_operator_tools formation_switcher column \\
        --ros-args -p target_spacing_m:=4.0
"""
import sys
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import Header

from combat_robot_msgs.msg import FormationCommand

from san_operator_tools.command_auth import CommandAuthGate


FORMATION_MAP = {
    "column":        FormationCommand.FORMATION_COLUMN,
    "line":          FormationCommand.FORMATION_LINE,
    "vshape":        FormationCommand.FORMATION_V_SHAPE,
    "v":             FormationCommand.FORMATION_V_SHAPE,
    "wedge":         FormationCommand.FORMATION_V_SHAPE,   # alias
    "diamond":       FormationCommand.FORMATION_DIAMOND,
    "echelon_left":  FormationCommand.FORMATION_ECHELON_LEFT,
    "echelon_right": FormationCommand.FORMATION_ECHELON_RIGHT,
    "box":           FormationCommand.FORMATION_BOX,
    "vee_inverted":  FormationCommand.FORMATION_VEE_INVERTED,
    "free_spread":   FormationCommand.FORMATION_FREE_SPREAD,
    "custom":        FormationCommand.FORMATION_FREE_SPREAD,  # alias
}


class FormationSwitcher(Node):
    """One-shot: publishes the formation change command 5 times
    (defence against packet loss in unreliable comm) then exits."""

    def __init__(self, formation_name: str):
        super().__init__("formation_switcher")
        self.declare_parameter("target_spacing_m", 3.0)
        self.declare_parameter("leader_heading_deg", 0.0)
        self.declare_parameter("leader_robot_id", 1)
        self.declare_parameter("repeats", 5)
        self.declare_parameter("repeat_delay_s", 0.1)

        key = formation_name.lower()
        if key not in FORMATION_MAP:
            raise ValueError(
                f"Unknown formation '{formation_name}'. "
                f"Choices: {sorted(FORMATION_MAP)}",
            )

        self.publisher_ = self.create_publisher(
            FormationCommand, "/swarm/formation_command", 10)

        # Phase 0 PR-D: gate kinetic command publish.
        self._auth = CommandAuthGate(self, "FormationCommand")

        msg = FormationCommand()
        msg.header = Header(frame_id="world")
        msg.command_id = int(time.time()) & 0xFFFFFFFF
        msg.sequence = msg.command_id
        msg.leader_robot_id = int(
            self.get_parameter("leader_robot_id").value)
        msg.formation = FORMATION_MAP[key]
        msg.target_spacing_m = float(
            self.get_parameter("target_spacing_m").value)
        msg.leader_heading_deg = float(
            self.get_parameter("leader_heading_deg").value)
        msg.timestamp_ms = int(time.time() * 1000)

        n = int(self.get_parameter("repeats").value)
        dt = float(self.get_parameter("repeat_delay_s").value)

        # Auth gate: refuse-to-publish if production_mode + empty operator.
        summary = (f"formation={formation_name}({msg.formation}) "
                   f"spacing={msg.target_spacing_m}m "
                   f"heading={msg.leader_heading_deg}° "
                   f"repeats={n}")
        if not self._auth.check_and_log(summary,
                                         target_id=msg.leader_robot_id):
            self.get_logger().error(
                "FormationSwitcher refused to publish — see auth gate "
                "ERROR above. No formation change sent.")
            # Schedule a quick shutdown so the operator sees the
            # failure clearly (avoid the legacy 5x-publish loop on a
            # refused command).
            self.create_timer(0.1, lambda: rclpy.shutdown())
            return

        for i in range(n):
            self.publisher_.publish(msg)
            self.get_logger().info(
                f"Sent FormationCommand[{i + 1}/{n}]: "
                f"{formation_name} ({msg.formation}) "
                f"spacing={msg.target_spacing_m} m")
            time.sleep(dt)

        # Schedule shutdown
        self.create_timer(0.1, lambda: rclpy.shutdown())


def main(args=None):
    if len(sys.argv) < 2:
        print("Usage: ros2 run san_operator_tools formation_switcher "
              "<column|line|wedge|diamond|custom>")
        sys.exit(1)
    formation = sys.argv[1]
    rclpy.init(args=args)
    try:
        node = FormationSwitcher(formation)
        rclpy.spin(node)
    except (KeyboardInterrupt, SystemExit):
        pass
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
