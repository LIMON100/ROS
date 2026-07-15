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
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    sim_share = get_package_share_directory("san_sim_gazebo")
    world_arg = LaunchConfiguration("world", default="tactical_test_world.sdf")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")

    # Path to world file
    world_path = PathJoinSubstitution([
        FindPackageShare("san_sim_gazebo"), "worlds", world_arg,
    ])

    # Bridge config
    bridge_config = os.path.join(sim_share, "config", "ros_gz_bridge.yaml")

    return LaunchDescription([
        DeclareLaunchArgument("world", default_value="tactical_test_world.sdf"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),

        # ── Gazebo Garden ──
        ExecuteProcess(
            cmd=["gz", "sim", "-r", world_path],
            output="screen",
        ),

        # ── Spawn 1 robot at origin ──
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(sim_share, "launch", "spawn_robot.launch.py"),
            ),
            launch_arguments={
                "robot_name": "san_combat_robot_1",
                "namespace":  "",
                "x": "0.0", "y": "0.0", "z": "0.5",
            }.items(),
        ),

        # ── ros_gz_bridge ──
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="ros_gz_bridge",
            arguments=["--ros-args", "-p", f"config_file:={bridge_config}"],
            parameters=[{"use_sim_time": use_sim_time}],
            output="screen",
        ),
    ])
