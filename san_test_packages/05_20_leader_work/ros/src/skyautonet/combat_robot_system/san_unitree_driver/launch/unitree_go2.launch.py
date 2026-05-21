# SAN v1.5 Phase 2-E Turn 2 — UnitreeGo2Node launch.
#
# Standalone usage (dev / smoke test):
#   ros2 launch san_unitree_driver unitree_go2.launch.py \
#       interface_name:=eth0
#
# Production: launched via squadron.launch.py when robot_role=leader.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    interface_name = LaunchConfiguration("interface_name")
    config_file = LaunchConfiguration("config_file")

    return LaunchDescription([
        DeclareLaunchArgument(
            "interface_name",
            default_value="eth0",
            description="Network interface for Unitree SDK DDS link.",
        ),
        DeclareLaunchArgument(
            "config_file",
            default_value=PathJoinSubstitution([
                FindPackageShare("san_unitree_driver"),
                "config", "unitree_go2.yaml",
            ]),
            description="ros__parameters yaml.",
        ),
        Node(
            package="san_unitree_driver",
            executable="unitree_go2_node",
            name="unitree_go2_node",
            output="screen",
            parameters=[
                config_file,
                {"interface_name": interface_name},
            ],
        ),
    ])
