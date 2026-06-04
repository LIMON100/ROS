# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Display robot URDF in RViz2.

Loads the xacro, publishes /robot_description, starts robot_state_publisher
and joint_state_publisher_gui, then RViz2 with a sensible default config.

Usage:
    ros2 launch san_description display.launch.py
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import Command, FindExecutable, LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory("san_description")
    xacro_path = os.path.join(pkg_share, "urdf", "san_robot.urdf.xacro")

    robot_description_content = Command([
        FindExecutable(name="xacro"), " ", xacro_path,
    ])

    use_gui = LaunchConfiguration("use_jsp_gui", default="true")

    return LaunchDescription([
        DeclareLaunchArgument("use_jsp_gui", default_value="true",
            description="Show joint_state_publisher_gui"),
        Node(
            package="robot_state_publisher",
            executable="robot_state_publisher",
            output="screen",
            parameters=[{"robot_description": robot_description_content}],
        ),
        Node(
            package="joint_state_publisher_gui",
            executable="joint_state_publisher_gui",
            condition=IfCondition(use_gui),
            output="screen",
        ),
        Node(
            package="rviz2",
            executable="rviz2",
            name="rviz2",
            output="screen",
            arguments=[],
        ),
    ])
