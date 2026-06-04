# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
SAN v1.5 — Full PDR demo (Gazebo sim + swarm + operator tools).

Top-level launch for visual demonstration of:
  - 8-robot swarm in a Gazebo world (tactical_test by default)
  - per-robot mission_node with SDD §6.1 Fallback BT
  - per-follower san_follower_tier + san_reroute_planner
  - san_formation Hub authority (Hungarian + 9 patterns)
  - san_surveillance Hub authority (360° sector + Pan-Tilt)
  - operator swarm_dashboard for live state visualization

Usage (during PDR demo):
    ros2 launch san_bringup pdr_demo.launch.py
    ros2 launch san_bringup pdr_demo.launch.py world:=obstacle_world.sdf
    # In separate terminal:
    ros2 run san_operator_tools formation_switcher diamond
    ros2 run san_operator_tools waypoint_sender
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument, IncludeLaunchDescription, GroupAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    sim_share = get_package_share_directory("san_sim_gazebo")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")
    with_dashboard = LaunchConfiguration("dashboard", default="true")

    return LaunchDescription([
        DeclareLaunchArgument("world",
                              default_value="tactical_test_world.sdf",
                              description="Gazebo SDF world file to load"),
        DeclareLaunchArgument("num_robots", default_value="8"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("dashboard", default_value="true",
                              description="Auto-launch swarm_dashboard"),

        # ─── Gazebo world + 8 robots ──────────────────────────────
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(sim_share, "launch", "swarm_sim.launch.py"),
            ),
            launch_arguments={
                "num_robots":   LaunchConfiguration("num_robots"),
                "world":        LaunchConfiguration("world"),
                "use_sim_time": use_sim_time,
            }.items(),
        ),

        # ─── Swarm dashboard (Tk GUI) ─────────────────────────────
        GroupAction(
            condition=IfCondition(with_dashboard),
            actions=[
                Node(
                    package="san_operator_tools",
                    executable="swarm_dashboard",
                    name="swarm_dashboard",
                    output="screen",
                    parameters=[{"use_sim_time": use_sim_time}],
                ),
            ],
        ),
    ])
