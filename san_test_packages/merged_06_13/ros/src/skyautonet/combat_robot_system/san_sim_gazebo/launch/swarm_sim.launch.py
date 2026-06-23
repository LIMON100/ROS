# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — 8-robot swarm simulation.

Spawns the Gazebo world + 8 robots in a V formation centered at
origin. Each robot gets its own namespace (/robot_1 … /robot_8) and
its own bridge node.

Usage:
    ros2 launch san_sim_gazebo swarm_sim.launch.py
    ros2 launch san_sim_gazebo swarm_sim.launch.py num_robots:=4
"""

import os
import math
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription, OpaqueFunction, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node

def _spawn_robots(context, *args, **kwargs):
    sim_share = get_package_share_directory("san_sim_gazebo")
    num_robots = int(LaunchConfiguration("num_robots").perform(context))
    pose_str = LaunchConfiguration("pose").perform(context)

    pose_parts = pose_str.split()
    b_x = float(pose_parts[0]) if len(pose_parts) > 0 else 0.0
    b_y = float(pose_parts[1]) if len(pose_parts) > 1 else 0.0
    b_z = float(pose_parts[2]) if len(pose_parts) > 2 else 0.5
    b_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0

    actions = []
    for i in range(1, num_robots + 1):
        if i == 1:
            spawn_x = b_x
            spawn_y = b_y
        else:
            row = i // 2
            side = 1 if i % 2 == 0 else -1
            spawn_dist_back = -3.0 * row
            spawn_dist_side = 2.5 * side
            spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
            spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))

        robot_ns = "" if i == 1 else f"robot_{i}"
        odom_frame = "odom" if i == 1 else f"robot_{i}/odom"

        # 1. Spawn Robot in Gazebo
        actions.append(
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(sim_share, "launch", "spawn_robot.launch.py"),
                ),
                # ---ARGUMENTS TO MATCH spawn_robot.launch.py ---
                launch_arguments={
                    "namespace": robot_ns,
                    "robot_name": f"san_combat_robot_{i}",
                    "x": str(spawn_x),
                    "y": str(spawn_y),
                    "z": str(b_z),
                }.items(),
                # ------------------------------------------------------
            )
        )

        actions.append(
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name=f'map_to_odom_{i}',
                arguments=[str(spawn_x), str(spawn_y), '0', '0', '0', str(b_yaw), 'map', odom_frame],
                remappings=[('/tf', '/tf'), ('/tf_static', '/tf_static')]
            )
        )
    return actions

def generate_launch_description():
    os.environ.setdefault("GZ_IP", "127.0.0.1")
    os.environ.setdefault("IGN_IP", "127.0.0.1")
    sim_share = get_package_share_directory("san_sim_gazebo")
    bridge_config = os.path.join(sim_share, "config", "ros_gz_bridge.yaml")
    pkg_description = get_package_share_directory('san_description')
    
    resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=[
            os.path.dirname(pkg_description),  # Path to find robot models
            ':',
            sim_share,                         # Path to find custom worlds
            ':',
            os.path.join(sim_share, 'models'), # Path to find custom map meshes
            ':',
            os.environ.get('GZ_SIM_RESOURCE_PATH', '') # Keep existing paths
        ]
    )

    return LaunchDescription([
        resource_path,
        DeclareLaunchArgument("num_robots", default_value="8"),
        DeclareLaunchArgument("world", default_value="obstacle_world.sdf"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("pose", default_value="0.0 0.0 0.5 0 0 0"),

        ExecuteProcess(
            cmd=["ign", "gazebo", "-r",
                PathJoinSubstitution([sim_share, "worlds", LaunchConfiguration("world")])],
            output="screen",
        ),
        OpaqueFunction(function=_spawn_robots),
    
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="ros_gz_bridge",
            arguments=["--ros-args", "-p", f"config_file:={bridge_config}"],
            output="screen",
        ),  
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="clock_bridge",
            arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
            output="screen",
        ),
    ])