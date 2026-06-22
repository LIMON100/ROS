# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Leader + single-Follower Gazebo demo.

Spawns two robots in Gazebo Harmonic (gz sim):

    robot 1  Leader   — spawns at the world origin (ns "")
    robot 2  Follower — spawns ``follow_distance`` m behind the Leader

The Leader is driven through a short waypoint route by
``san_operator_tools/leader_path_nav``, which also publishes a DASHED path
marker (``/leader/waypoint_path``) showing the route to the goal. The
Follower holds a fixed distance directly behind the Leader using
``san_follower_tier/follower_pursuit_node`` (offset_side = 0).

Because each robot's Gazebo DiffDrive odom frame originates at its own spawn
point, the Follower is given ``own_odom_offset_x = -follow_distance`` so its
pose is reported in the Leader's world frame (see follower_pursuit_node).

RViz comes up with the two robot models, TF, and the dashed waypoint path.
Gazebo itself shows the robots driving in the world.

Usage:
    ros2 launch san_sim_gazebo leader_follower_demo.launch.py
    ros2 launch san_sim_gazebo leader_follower_demo.launch.py follow_distance:=5.0
    ros2 launch san_sim_gazebo leader_follower_demo.launch.py headless:=true rviz:=false
"""
import math
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
    OpaqueFunction,
    SetEnvironmentVariable,
    TimerAction,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node


# Rectangular loop (legacy): sharp 90-degree corners, stays within +/-10 m.
_LOOP_ROUTE = [10.0, 0.0, 10.0, 10.0, -10.0, 10.0,
               -10.0, -10.0, 10.0, -10.0, 0.0, 0.0]


def _figure8_route(radius=6.0, step_m=1.8):
    """Dense waypoints tracing a figure-8 = two tangent circles through the
    origin (the leader's spawn). Left loop (CCW, +y) then right loop (CW, -y),
    both tangent to +x at the origin. Sampled finely so leader_path_nav's
    turn-then-go controller drives a smooth curve instead of cornering."""
    n = max(8, int(round(2.0 * math.pi * radius / step_m)))
    pts = []
    for i in range(1, n + 1):                       # left circle, centre (0,+R)
        s = 2.0 * math.pi * i / n
        pts += [radius * math.sin(s), radius * (1.0 - math.cos(s))]
    for i in range(1, n + 1):                       # right circle, centre (0,-R)
        s = 2.0 * math.pi * i / n
        pts += [radius * math.sin(s), -radius * (1.0 - math.cos(s))]
    return pts


def _route_waypoints(route):
    return _figure8_route() if route == "figure8" else _LOOP_ROUTE


def _setup(context, *args, **kwargs):
    sim_share = get_package_share_directory("san_sim_gazebo")
    pkg_description = get_package_share_directory("san_description")

    follow_distance = float(LaunchConfiguration("follow_distance").perform(context))
    use_sim_time = LaunchConfiguration("use_sim_time").perform(context) == "true"
    route = LaunchConfiguration("route").perform(context)
    waypoints = _route_waypoints(route)
    rviz_cfg = os.path.join(sim_share, "rviz", "leader_follower.rviz")

    leader_x, leader_y = 0.0, 0.0
    follower_x, follower_y = -follow_distance, 0.0

    spawn_leader = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(sim_share, "launch", "spawn_robot.launch.py")),
        launch_arguments={
            "namespace": "",
            "robot_name": "san_combat_robot_1",
            "x": str(leader_x), "y": str(leader_y), "z": "0.5",
            "use_sim_time": "true" if use_sim_time else "false",
        }.items(),
    )
    spawn_follower = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(sim_share, "launch", "spawn_robot.launch.py")),
        launch_arguments={
            "namespace": "robot_2",
            "robot_name": "san_combat_robot_2",
            "x": str(follower_x), "y": str(follower_y), "z": "0.5",
            "use_sim_time": "true" if use_sim_time else "false",
        }.items(),
    )

    # Common world frame: map -> each robot's odom (sim ground truth).
    tf_leader = Node(
        package="tf2_ros", executable="static_transform_publisher",
        name="map_to_odom_leader", output="screen",
        arguments=[str(leader_x), str(leader_y), "0", "0", "0", "0",
                   "map", "odom"])
    # OdometryPublisher reports each robot's pose in the WORLD frame (not
    # spawn-relative), so every odom frame coincides with map → identity TF.
    tf_follower = Node(
        package="tf2_ros", executable="static_transform_publisher",
        name="map_to_odom_follower", output="screen",
        arguments=["0", "0", "0", "0", "0", "0",
                   "map", "robot_2/odom"])

    # Leader waypoint driver + dashed path marker (world frame == map).
    # Route from the 'route' arg: figure8 (smooth 8 through the origin,
    # x∈±6 m / y∈±12 m) or loop (rectangle). Both stay inside the ±22 m walls,
    # starting at the centre so the robots stay in view the whole run.
    leader_nav = Node(
        package="san_operator_tools", executable="leader_path_nav",
        name="leader_path_nav", output="screen",
        parameters=[{
            "use_sim_time": use_sim_time,
            "goal_frame": "map",
            "cruise_speed_mps": 0.6,
            "max_angular_rps": 1.2,
            # Route selected by the 'route' launch arg: 'figure8' (default,
            # smooth two-circle figure-8 through the origin) or 'loop' (legacy
            # rectangular loop). Both stay well inside the ±22 m arena walls.
            "waypoints": waypoints,
        }])

    # Follower: hold follow_distance directly behind the leader.
    follower = Node(
        package="san_follower_tier", executable="follower_pursuit_node",
        name="follower_pursuit_node", namespace="robot_2", output="screen",
        parameters=[{
            "use_sim_time": use_sim_time,
            "robot_id": 2,
            "leader_odom_topic": "/odom",
            "offset_back_m": -follow_distance,   # row=1 -> behind by this much
            "offset_side_m": 0.0,                # directly behind
            # OdometryPublisher odom is already world-frame, so no offset.
            "own_odom_offset_x": 0.0,
            "own_odom_offset_y": 0.0,
            "own_odom_offset_yaw": 0.0,
            "stop_distance_m": 0.5,
            "max_linear_mps": 1.0,               # out-run leader (0.8) to close the gap
            "max_angular_rps": 1.5,
            "min_leader_dist_m": 3.5,            # never rear-end / push the leader
            "path_follow": True,                 # trail the leader's actual track

        }])

    rviz = Node(
        package="rviz2", executable="rviz2", name="rviz2",
        arguments=["-d", rviz_cfg],
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(LaunchConfiguration("rviz")),
        output="screen")

    # Spawns + TF up front; control nodes after a delay so odom is flowing.
    return [
        spawn_leader,
        spawn_follower,
        tf_leader,
        tf_follower,
        rviz,
        TimerAction(period=8.0, actions=[leader_nav, follower]),
    ]


def generate_launch_description():
    # gz-transport on WSL must be pinned to loopback or discovery hangs
    # (mirrors swarm_sim.launch.py / PR #249).
    os.environ.setdefault("GZ_IP", "127.0.0.1")
    os.environ.setdefault("IGN_IP", "127.0.0.1")

    sim_share = get_package_share_directory("san_sim_gazebo")
    pkg_description = get_package_share_directory("san_description")

    resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.path.dirname(pkg_description),       # robot models (package://)
            ":", sim_share,                         # custom worlds
            ":", os.path.join(sim_share, "models"),  # custom map meshes
            ":", os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
        ])

    world_path = PathJoinSubstitution(
        [sim_share, "worlds", LaunchConfiguration("world")])
    headless = LaunchConfiguration("headless")

    # Full default gz GUI config (so rendering is unbroken) with only the
    # camera moved to an overhead view that frames the whole ±10 m loop, so
    # both robots stay on screen while they drive (the default camera only
    # looks down the +x axis and loses them mid-loop).
    gui_config = os.path.join(sim_share, "config", "leader_follower_gui.config")

    gz_gui = ExecuteProcess(
        cmd=["gz", "sim", "-r", "--gui-config", gui_config, world_path],
        output="screen", condition=UnlessCondition(headless))
    gz_headless = ExecuteProcess(
        cmd=["gz", "sim", "-s", "-r", "--headless-rendering", world_path],
        output="screen", condition=IfCondition(headless))

    # /clock bridge so use_sim_time nodes get sim time.
    clock_bridge = Node(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="clock_bridge", output="screen",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"])

    return LaunchDescription([
        # Sensor-free world (no gz-sim-sensors-system) so RTF stays ~1.0 on
        # GPU-less hosts; pass world:=empty_world.sdf to restore full sensors.
        DeclareLaunchArgument("world", default_value="leader_follower_world.sdf"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("follow_distance", default_value="5.0"),
        DeclareLaunchArgument("headless", default_value="false"),
        DeclareLaunchArgument("rviz", default_value="true"),
        # Leader route: 'figure8' (smooth curved 8) or 'loop' (rectangle).
        DeclareLaunchArgument("route", default_value="figure8"),
        resource_path,
        gz_gui,
        gz_headless,
        clock_bridge,
        OpaqueFunction(function=_setup),
    ])
