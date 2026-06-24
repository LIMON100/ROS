# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_comm", executable="comm_uplink_node",
            name="comm_uplink_node", output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_comm"),
                "config", "comm_uplink.yaml",
            ])],
        ),
    ])
