# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
SAN v1.5 — GPS jump injector launch.

Disturbs /robot_N/gps/fix with realistic GPS pathologies so the
Limon dual-EKF patches can be exercised. Default profile = 8-robot
swarm, jump at 8s, baseline noise, no dropout.
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    robots = LaunchConfiguration("robots", default="8")
    return LaunchDescription([
        DeclareLaunchArgument("robots", default_value="8"),
        Node(
            package="san_sim_gazebo_helpers",
            executable="gps_jump_injector",
            name="gps_jump_injector",
            output="screen",
            parameters=[{
                "robots":            robots,
                "enable_jump":       True,
                "jump_at_s":         8.0,
                "jump_east_m":       2.5,
                "jump_north_m":      0.5,
                "jump_recovery_s":   2.0,
                "enable_noise":      True,
                "noise_east_std_m":  0.3,
                "noise_north_std_m": 0.3,
                "noise_alt_std_m":   0.6,
                "enable_dropout":    False,
                "dropout_at_s":      15.0,
                "dropout_duration_s": 3.0,
                "enable_drift":      False,
            }],
        ),
    ])
