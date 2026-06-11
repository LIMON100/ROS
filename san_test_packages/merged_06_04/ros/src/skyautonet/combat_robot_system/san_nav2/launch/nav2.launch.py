# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Nav2 navigation stack.

Brings up Nav2 (planner + controller + behaviors + lifecycle manager)
with the SAN-tuned params. Pairs with san_localization for pose.

Usage:
    ros2 launch san_nav2 nav2.launch.py
    ros2 launch san_nav2 nav2.launch.py params_file:=...
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument, IncludeLaunchDescription,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_share = get_package_share_directory("san_nav2")
    nav2_bringup = get_package_share_directory("nav2_bringup")

    return LaunchDescription([
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument(
            "params_file",
            default_value=os.path.join(pkg_share, "config", "nav2_params.yaml")),
        DeclareLaunchArgument(
            "autostart", default_value="true"),

        # Include the standard Nav2 bringup with our params
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup, "launch", "navigation_launch.py"),
            ),
            launch_arguments={
                "use_sim_time": LaunchConfiguration("use_sim_time"),
                "params_file":  LaunchConfiguration("params_file"),
                "autostart":    LaunchConfiguration("autostart"),
            }.items(),
        ),
    ])
