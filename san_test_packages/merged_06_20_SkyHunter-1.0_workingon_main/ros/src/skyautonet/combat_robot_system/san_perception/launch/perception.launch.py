# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN v1.5 Phase 2-E Turn 11-12 — Perception standalone launch.
from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_perception",
            executable="perception_node",
            name="perception_node",
            output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_perception"),
                "config", "perception.yaml",
            ])],
            remappings=[
                ("camera_compressed", "/imx678_camera_node/image_compressed"),
                ("thermal_image",     "/thermal_camera_node/image"),
                ("pose",              "/pose"),
            ],
        ),
    ])
