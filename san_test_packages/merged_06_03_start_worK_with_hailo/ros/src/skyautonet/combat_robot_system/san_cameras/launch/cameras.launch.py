# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN v1.5 Phase 2-E Turn 6 — Both camera nodes launch.
from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg = FindPackageShare("san_cameras")
    return LaunchDescription([
        Node(
            package="san_cameras", executable="imx678_camera_node",
            name="imx678_camera_node", output="screen",
            parameters=[PathJoinSubstitution([pkg, "config", "imx678.yaml"])],
        ),
        Node(
            package="san_cameras", executable="thermal_camera_node",
            name="thermal_camera_node", output="screen",
            parameters=[PathJoinSubstitution([pkg, "config", "thermal.yaml"])],
        ),
    ])
