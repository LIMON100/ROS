# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Single-robot Gazebo simulation entry point.

Spawns Gazebo Garden with the chosen world + 1 robot at origin.
Bridges sim ↔ ROS 2 topics for cmd_vel / odom / TF / sensors.

Usage:
    ros2 launch san_sim_gazebo sim.launch.py
    ros2 launch san_sim_gazebo sim.launch.py world:=obstacle_world.sdf
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():

    os.environ.setdefault("GZ_IP", "127.0.0.1")
    os.environ.setdefault("IGN_IP", "127.0.0.1")
    
    sim_share = get_package_share_directory("san_sim_gazebo")
    world_arg = LaunchConfiguration("world", default="tactical_test_world.sdf")
    headless = LaunchConfiguration("headless", default="false")

    # Path to world file
    world_path = PathJoinSubstitution([
        FindPackageShare("san_sim_gazebo"), "worlds", world_arg,
    ])

    return LaunchDescription([
        DeclareLaunchArgument("world", default_value="tactical_test_world.sdf"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument(
            "headless", default_value="false",
            description="Run gz server-only (no GUI) — for CI / headless test"),

        # ── Gazebo: GUI (default) ──
        ExecuteProcess(
            cmd=["gz", "sim", "-r", world_path],
            output="screen",
            condition=UnlessCondition(headless),
        ),
        # ── Gazebo: server-only (headless) ──
        # --headless-rendering renders GPU sensors (gpu_lidar, cameras)
        # off-screen via EGL so /scan/points is produced without a display
        # (uses the WSLg GPU /dev/dxg, or Mesa llvmpipe in software). Nav2's
        # costmap needs those points to establish free space.
        ExecuteProcess(
            cmd=["gz", "sim", "-s", "-r", "--headless-rendering", world_path],
            output="screen",
            condition=IfCondition(headless),
        ),

        # ── Spawn 1 robot at origin ──
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(sim_share, "launch", "spawn_robot.launch.py"),
            ),
            launch_arguments={
                "robot_name": "san_combat_robot_1",
                # Root namespace for the single-robot case: topics become
                # /cmd_vel, /odom and TF frames odom/base_footprint, which
                # match the stock nav2_params.yaml directly (sim_nav).
                "namespace":  "",
                "x": "0.0", "y": "0.0", "z": "0.5",
            }.items(),
        ),
        # NOTE: spawn_robot.launch.py creates the ros_gz_bridge itself
        # (per-namespace), so no separate bridge node is needed here.
    ])
