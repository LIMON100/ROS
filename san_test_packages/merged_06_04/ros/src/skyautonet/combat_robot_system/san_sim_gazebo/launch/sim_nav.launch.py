# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Single-robot (Leader) Gazebo + Nav2 waypoint demo.

Makes the Leader DRIVE to operator waypoints in simulation. The plain
sim.launch.py only brings up Gazebo + model + ros_gz_bridge; nothing
publishes cmd_vel, so nothing moves. This launch adds the control chain:

    waypoint_sender ─/swarm/waypoint_command─▶ waypoint_to_nav2
        ─NavigateThroughPoses─▶ Nav2 (planner+controller) ─/cmd_vel─▶
        ros_gz_bridge ─▶ Gazebo DiffDrive ─▶ robot moves.

The single robot spawns in the ROOT namespace (sim.launch.py), so its
topics are /cmd_vel, /odom and TF frames are odom/base_footprint — a
direct match for the stock san_nav2/nav2_params.yaml. Only two params are
re-pointed for sim: odom_topic -> the bridged /odom, and the costmap
pointcloud source -> the bridged /scan/points (no EKF, no topic relays).
The global costmap is a rolling window (no static layer), so no map_server
is needed. Multi-robot is handled separately in swarm_nav.launch.py.

Usage:
    ros2 launch san_sim_gazebo sim_nav.launch.py
    ros2 launch san_sim_gazebo sim_nav.launch.py headless:=true world:=empty_world.sdf
"""
import os
import tempfile

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    OpaqueFunction,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def make_sim_nav2_params(base_params: str) -> str:
    """Re-point the stock Nav2 params at the sim's bridged topics. Frames
    are already the stock odom/base_footprint (root-namespace robot), so
    only the data sources change:
      - odom_topic   -> the bridged gz /odom (no EKF in the sim)
      - obstacle src -> the bridged gz lidar /scan/points

    NOTE: the global costmap is a rolling VoxelLayer fed by the lidar. In a
    GPU-rendered Gazebo the lidar populates it and Nav2 plans/drives. A
    fully headless host (no GPU) renders no lidar points, so the costmap
    has no free space established and the planner reports "no valid
    path" / "start in lethal space" — a host limitation, not a wiring bug.
    """
    with open(base_params) as f:
        cfg = yaml.safe_load(f)

    for _node, p in cfg.items():
        rp = p.get("ros__parameters", {}) if isinstance(p, dict) else {}
        if "odom_topic" in rp:
            rp["odom_topic"] = "odom"
    for layer in (cfg["global_costmap"]["global_costmap"]["ros__parameters"],
                  cfg["local_costmap"]["local_costmap"]["ros__parameters"]):
        ol = layer.get("obstacle_layer", {})
        pc = ol.get("pointcloud")
        if isinstance(pc, dict):
            pc["topic"] = "/scan/points"
            # The stock params mark points from -10..+10 m as obstacles,
            # which includes the GROUND — once the (now-rendered) lidar
            # produces points the whole floor becomes lethal and the robot
            # can only spin in place. Gate to a band above the ground so
            # the floor is free and real obstacles (0.3..2.5 m) still mark.
            pc["min_obstacle_height"] = 0.3
            pc["max_obstacle_height"] = 2.5

    out = os.path.join(tempfile.gettempdir(), "sim_nav2_params.yaml")
    with open(out, "w") as f:
        yaml.safe_dump(cfg, f, default_flow_style=False)
    return out


def launch_setup(context, *args, **kwargs):
    use_sim = LaunchConfiguration("use_sim_time").perform(context) == "true"
    nav2_share = get_package_share_directory("san_nav2")
    nav2_bringup = get_package_share_directory("nav2_bringup")
    params = make_sim_nav2_params(
        os.path.join(nav2_share, "config", "nav2_params.yaml"))

    # static map -> odom (sim ground-truth identity)
    map_to_odom = Node(
        package="tf2_ros", executable="static_transform_publisher",
        name="sim_map_to_odom", output="screen",
        parameters=[{"use_sim_time": use_sim}],
        arguments=["0", "0", "0", "0", "0", "0", "map", "odom"],
    )
    # Nav2 (root namespace) via the standard bringup with our params.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup, "launch", "navigation_launch.py")),
        launch_arguments={
            "use_sim_time": "true" if use_sim else "false",
            "autostart": "True",
            "params_file": params,
            "use_composition": "False",
        }.items(),
    )
    # waypoint -> Nav2 bridge (announces Leader so waypoint_sender publishes)
    wp_bridge = Node(
        package="san_operator_tools", executable="waypoint_to_nav2",
        name="waypoint_to_nav2", output="screen",
        parameters=[{"use_sim_time": use_sim, "robot_id": 1,
                     "goal_frame": "map", "announce_leader": True}],
    )
    # Stagger so gz + bridge + TF are up before Nav2 activates.
    return [TimerAction(period=6.0, actions=[map_to_odom, nav2, wp_bridge])]


def generate_launch_description():
    sim_share = get_package_share_directory("san_sim_gazebo")

    use_sim_time = LaunchConfiguration("use_sim_time")
    world_arg = LaunchConfiguration("world")
    headless = LaunchConfiguration("headless")
    start_sender = LaunchConfiguration("start_waypoint_sender")

    sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(sim_share, "launch", "sim.launch.py")),
        launch_arguments={"world": world_arg, "use_sim_time": use_sim_time,
                          "headless": headless}.items(),
    )
    clock_bridge = Node(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="clock_bridge", output="screen",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
    )
    wp_sender = Node(
        package="san_operator_tools", executable="waypoint_sender",
        name="waypoint_sender", output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(start_sender),
    )

    return LaunchDescription([
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("world", default_value="empty_world.sdf"),
        DeclareLaunchArgument("start_waypoint_sender", default_value="true"),
        DeclareLaunchArgument("headless", default_value="false"),
        sim,
        clock_bridge,
        TimerAction(period=5.0, actions=[wp_sender]),
        OpaqueFunction(function=launch_setup),
    ])
