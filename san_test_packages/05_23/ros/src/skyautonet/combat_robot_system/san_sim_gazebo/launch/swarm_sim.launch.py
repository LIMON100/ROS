"""SAN v1.5 — 8-robot swarm simulation.

Spawns the Gazebo world + 8 robots in a V formation centered at
origin. Each robot gets its own namespace (/robot_1 … /robot_8) and
its own bridge node.

Usage:
    ros2 launch san_sim_gazebo swarm_sim.launch.py
    ros2 launch san_sim_gazebo swarm_sim.launch.py num_robots:=4
"""
# import os

# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import (
#     DeclareLaunchArgument,
#     ExecuteProcess,
#     IncludeLaunchDescription,
#     OpaqueFunction,
# )
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node


# # Initial spawn positions for an 8-robot V formation around origin.
# # Index 0 is Leader (front), the rest form the V tails.
# SPAWN_POSES = [
#     (0.0,   0.0),   # 1 Leader
#     (-3.0,  2.5),   # 2 left wing 1
#     (-3.0, -2.5),   # 3 right wing 1
#     (-6.0,  5.0),   # 4 left wing 2
#     (-6.0, -5.0),   # 5 right wing 2
#     (-9.0,  7.5),   # 6 left wing 3
#     (-9.0, -7.5),   # 7 right wing 3
#     (-12.0, 0.0),   # 8 tail (Hub UGV)
# ]


# def _spawn_robots(context, *args, **kwargs):
#     sim_share = get_package_share_directory("san_sim_gazebo")
#     num_robots = int(LaunchConfiguration("num_robots").perform(context))
#     actions = []
#     for rid in range(1, min(num_robots, len(SPAWN_POSES)) + 1):
#         x, y = SPAWN_POSES[rid - 1]
#         actions.append(
#             IncludeLaunchDescription(
#                 PythonLaunchDescriptionSource(
#                     os.path.join(sim_share, "launch", "spawn_robot.launch.py"),
#                 ),
#                 launch_arguments={
#                     "robot_name": f"san_combat_robot_{rid}",
#                     "namespace":  f"/robot_{rid}",
#                     "x": str(x), "y": str(y), "z": "0.5",
#                 }.items(),
#             ),
#         )
#     return actions


# def generate_launch_description():
#     sim_share = get_package_share_directory("san_sim_gazebo")
#     world_arg = LaunchConfiguration("world", default="empty_world.sdf")
#     bridge_config = os.path.join(sim_share, "config", "ros_gz_bridge.yaml")

#     return LaunchDescription([
#         DeclareLaunchArgument("num_robots", default_value="8"),
#         DeclareLaunchArgument("world",
#             default_value="empty_world.sdf"),
#         DeclareLaunchArgument("use_sim_time", default_value="true"),

#         ExecuteProcess(
#             cmd=["ign", "gazebo", "-r",
#                   os.path.join(sim_share, "worlds", "empty_world.sdf")],
#             output="screen",
#         ),

#         # Spawn N robots
#         OpaqueFunction(function=_spawn_robots),

#         # Shared bridge
#         Node(
#             package="ros_gz_bridge",
#             executable="parameter_bridge",
#             name="ros_gz_bridge",
#             arguments=["--ros-args", "-p", f"config_file:={bridge_config}"],
#             output="screen",
#         ),
#     ])



# import os
# import math
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription, OpaqueFunction, SetEnvironmentVariable
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
# from launch_ros.actions import Node

# def _spawn_robots(context, *args, **kwargs):
#     sim_share = get_package_share_directory("san_sim_gazebo")
#     num_robots = int(LaunchConfiguration("num_robots").perform(context))
#     pose_str = LaunchConfiguration("pose").perform(context)

#     # Parse your custom pose string
#     pose_parts = pose_str.split()
#     b_x = float(pose_parts[0]) if len(pose_parts) > 0 else 0.0
#     b_y = float(pose_parts[1]) if len(pose_parts) > 1 else 0.0
#     b_z = float(pose_parts[2]) if len(pose_parts) > 2 else 0.5
#     b_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0

#     actions = []
#     for i in range(1, num_robots + 1):
#         # Leader is robot 1, followers are 2+
#         if i == 1:
#             spawn_x = b_x
#             spawn_y = b_y
#         else:
#             row = i // 2
#             side = 1 if i % 2 == 0 else -1
#             spawn_dist_back = -3.0 * row
#             spawn_dist_side = 2.5 * side
            
#             # Apply Yaw rotation to the V-Shape offset
#             spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
#             spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))

#         actions.append(
#             IncludeLaunchDescription(
#                 PythonLaunchDescriptionSource(
#                     os.path.join(sim_share, "launch", "spawn_robot.launch.py"),
#                 ),

#                 launch_arguments={
#                     "robot_ns": "" if i == 1 else f"robot_{i}",
#                     "pose":     f"{spawn_x} {spawn_y} {b_z} 0 0 {b_yaw}",
#                 }.items(),

#             ),
#         )
#     return actions


# def generate_launch_description():
#     sim_share = get_package_share_directory("san_sim_gazebo")
#     bridge_config = os.path.join(sim_share, "config", "ros_gz_bridge.yaml")

#     pkg_description = get_package_share_directory('san_description')
    
#     # Set the resource path so Gazebo can find the meshes
#     # We point to the directory ABOVE san_description so 'model://san_description' works
#     resource_path = SetEnvironmentVariable(
#         name='GZ_SIM_RESOURCE_PATH',
#         value=[os.path.dirname(pkg_description)]
#     )

#     return LaunchDescription([
#         resource_path,
#         DeclareLaunchArgument("num_robots", default_value="8"),
#         DeclareLaunchArgument("world", default_value="obstacle_world.sdf"),
#         DeclareLaunchArgument("use_sim_time", default_value="true"),
#         DeclareLaunchArgument("pose", default_value="0.0 0.0 0.5 0 0 0"),

#         ExecuteProcess(
#             cmd=["ign", "gazebo", "-r",
#                 PathJoinSubstitution([sim_share, "worlds", LaunchConfiguration("world")])],
#             output="screen",
#         ),

#         OpaqueFunction(function=_spawn_robots),

#         Node(
#             package="ros_gz_bridge",
#             executable="parameter_bridge",
#             name="ros_gz_bridge",
#             arguments=["--ros-args", "-p", f"config_file:={bridge_config}"],
#             output="screen",
#         ),
#     ])


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
                launch_arguments={
                    "robot_ns": robot_ns,
                    "pose":     f"{spawn_x} {spawn_y} {b_z} 0 0 {b_yaw}",
                }.items(),
            )
        )

        # 2. CRITICAL FIX: Publish Map -> Odom TF for each robot based on spawn offset
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
    sim_share = get_package_share_directory("san_sim_gazebo")
    bridge_config = os.path.join(sim_share, "config", "ros_gz_bridge.yaml")
    pkg_description = get_package_share_directory('san_description')
    
    resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=[os.path.dirname(pkg_description)]
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
    ])