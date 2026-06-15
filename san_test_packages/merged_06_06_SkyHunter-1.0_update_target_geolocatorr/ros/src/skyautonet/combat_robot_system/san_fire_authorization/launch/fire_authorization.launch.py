# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN v1.5 PHASE 9 — FireAuthorizationNode launch.
# Usage:
#   ros2 launch san_fire_authorization fire_authorization.launch.py
#   ros2 launch san_fire_authorization fire_authorization.launch.py \
#       secret_path:=/run/secrets/mesh_secret.bin \
#       audit_log_path:=/var/log/san/fire_audit.log

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    secret_path = LaunchConfiguration("secret_path")
    audit_log_path = LaunchConfiguration("audit_log_path")
    config_file = LaunchConfiguration("config_file")

    return LaunchDescription([
        DeclareLaunchArgument(
            "secret_path",
            default_value="/etc/san/mesh_secret.bin",
            description="32-byte HMAC mesh shared secret, mode 0400.",
        ),
        DeclareLaunchArgument(
            "audit_log_path",
            default_value="/var/log/san/fire_audit.log",
            description="Append-only JSON Lines audit log path.",
        ),
        DeclareLaunchArgument(
            "config_file",
            default_value=PathJoinSubstitution([
                FindPackageShare("san_fire_authorization"),
                "config",
                "fire_authorization.yaml",
            ]),
            description="ros__parameters yaml.",
        ),
        Node(
            package="san_fire_authorization",
            executable="fire_authorization_node",
            name="fire_authorization_node",
            output="screen",
            parameters=[
                config_file,
                {
                    "secret_path":    secret_path,
                    "audit_log_path": audit_log_path,
                },
            ],
        ),
    ])
