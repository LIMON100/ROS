# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Multi-robot Gazebo + per-robot Nav2 waypoint navigation.

Scales the single-robot sim_nav.launch.py to an N-robot swarm. Each robot
(robot_1 .. robot_N) spawns in its own namespace and gets its own Nav2
stack, so the control chain per robot is:

    waypoint_to_nav2 (robot_i) ─NavigateThroughPoses─▶ /robot_i Nav2
        ─/robot_i/cmd_vel─▶ ros_gz_bridge ─▶ DiffDrive ─▶ robot moves.

Everything proven for the single robot is reused: the gz `--headless-
rendering` flag (so the gpu_lidar publishes /scan/points headless), the
obstacle-height fix (so the floor is not marked lethal), and Nav2 param
binding. The extra multi-robot piece is per-robot TF-frame prefixing
(robot_i/base_footprint, robot_i/odom) plus RewrittenYaml(root_key=ns) so
the flat nav2_params bind to the namespaced nodes.

waypoint_sender drives the current Leader (robot_1). Followers' Nav2 stacks
are up and can be commanded directly:
    ros2 action send_goal /robot_2/navigate_to_pose nav2_msgs/action/NavigateToPose ...

Usage:
    ros2 launch san_sim_gazebo swarm_nav.launch.py num_robots:=8
    ros2 launch san_sim_gazebo swarm_nav.launch.py num_robots:=2 headless:=true
"""
import os
import tempfile

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    GroupAction,
    IncludeLaunchDescription,
    OpaqueFunction,
    TimerAction,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.substitutions import FindPackageShare
from nav2_common.launch import RewrittenYaml


# Initial spawn positions for an 8-robot V formation around origin.
SPAWN_POSES = [
    (0.0,   0.0),    # 1 Leader (front)
    (-3.0,  2.5),    # 2 left wing 1
    (-3.0, -2.5),    # 3 right wing 1
    (-6.0,  5.0),    # 4 left wing 2
    (-6.0, -5.0),    # 5 right wing 2
    (-9.0,  7.5),    # 6 left wing 3
    (-9.0, -7.5),    # 7 right wing 3
    (-12.0, 0.0),    # 8 tail (Hub UGV)
]


def make_robot_nav2_params(ns: str, base_params: str) -> str:
    """Per-robot Nav2 params: namespace the TF frames + re-point the sim
    data sources (same obstacle-height / odom / scan fixes as sim_nav)."""
    with open(base_params) as f:
        cfg = yaml.safe_load(f)

    base = f"{ns}/base_footprint"
    odom = f"{ns}/odom"

    def setp(node, key, val):
        cfg.get(node, {}).get("ros__parameters", {})[key] = val

    setp("bt_navigator", "robot_base_frame", base)
    gc = cfg["global_costmap"]["global_costmap"]["ros__parameters"]
    gc["robot_base_frame"] = base            # global_frame stays "map"
    lc = cfg["local_costmap"]["local_costmap"]["ros__parameters"]
    lc["global_frame"] = odom
    lc["robot_base_frame"] = base
    setp("behavior_server", "global_frame", odom)
    setp("behavior_server", "robot_base_frame", base)

    for _node, p in cfg.items():
        rp = p.get("ros__parameters", {}) if isinstance(p, dict) else {}
        if "odom_topic" in rp:
            rp["odom_topic"] = "odom"        # relative -> /<ns>/odom
    for layer in (gc, lc):
        pc = layer.get("obstacle_layer", {}).get("pointcloud")
        if isinstance(pc, dict):
            pc["topic"] = f"/{ns}/scan/points"
            pc["min_obstacle_height"] = 0.3   # do not mark the ground
            pc["max_obstacle_height"] = 2.5

    out = os.path.join(tempfile.gettempdir(), f"{ns}_nav2_params.yaml")
    with open(out, "w") as f:
        yaml.safe_dump(cfg, f, default_flow_style=False)
    return out


def _nav(pkg, exe, name, params):
    return Node(package=pkg, executable=exe, name=name, output="screen",
                parameters=[params])


def launch_setup(context, *args, **kwargs):
    num = int(LaunchConfiguration("num_robots").perform(context))
    use_sim = LaunchConfiguration("use_sim_time").perform(context) == "true"
    sim_share = get_package_share_directory("san_sim_gazebo")
    nav2_share = get_package_share_directory("san_nav2")
    base_params = os.path.join(nav2_share, "config", "nav2_params.yaml")

    actions = []
    for rid in range(1, min(num, len(SPAWN_POSES)) + 1):
        ns = f"robot_{rid}"
        x, y = SPAWN_POSES[rid - 1]

        # Spawn the robot model in its namespace.
        actions.append(IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(sim_share, "launch", "spawn_robot.launch.py")),
            launch_arguments={"robot_name": f"san_combat_robot_{rid}",
                              "namespace": f"/{ns}",
                              "x": str(x), "y": str(y), "z": "0.5",
                              "use_sim_time": "true" if use_sim else "false"}.items(),
        ))

        # Per-robot Nav2 params: frames namespaced, then RewrittenYaml nests
        # them under the namespace key so they bind to the /ns/<node>s.
        params = RewrittenYaml(
            source_file=make_robot_nav2_params(ns, base_params),
            root_key=ns, param_rewrites={"use_sim_time": "true" if use_sim else "false"},
            convert_types=True)

        nav_group = GroupAction([
            PushRosNamespace(ns),
            # global TF map -> <ns>/odom (sim ground truth)
            Node(package="tf2_ros", executable="static_transform_publisher",
                 name="sim_map_to_odom", output="screen",
                 parameters=[{"use_sim_time": use_sim}],
                 arguments=["0", "0", "0", "0", "0", "0", "map", f"{ns}/odom"]),
            _nav("nav2_controller", "controller_server", "controller_server", params),
            _nav("nav2_planner", "planner_server", "planner_server", params),
            _nav("nav2_behaviors", "behavior_server", "behavior_server", params),
            _nav("nav2_bt_navigator", "bt_navigator", "bt_navigator", params),
            _nav("nav2_smoother", "smoother_server", "smoother_server", params),
            _nav("nav2_waypoint_follower", "waypoint_follower",
                 "waypoint_follower", params),
            Node(package="nav2_lifecycle_manager", executable="lifecycle_manager",
                 name="lifecycle_manager_navigation", output="screen",
                 parameters=[{"use_sim_time": use_sim, "autostart": True,
                              "node_names": ["controller_server", "planner_server",
                                             "behavior_server", "bt_navigator",
                                             "smoother_server", "waypoint_follower"]}]),
            Node(package="san_operator_tools", executable="waypoint_to_nav2",
                 name="waypoint_to_nav2", output="screen",
                 parameters=[{"use_sim_time": use_sim, "robot_id": rid,
                              "goal_frame": "map",
                              # only the Leader (robot_1) announces, so the
                              # shared waypoint_sender unblocks once.
                              "announce_leader": rid == 1}]),
        ])
        # Stagger Nav2 bringup per robot so N lifecycle managers don't all
        # configure/activate at the same instant (headless hosts are
        # resource-tight; simultaneous bringup can stall later robots).
        actions.append(TimerAction(period=7.0 + (rid - 1) * 4.0,
                                   actions=[nav_group]))

    return actions


def generate_launch_description():
    use_sim_time = LaunchConfiguration("use_sim_time")
    world_arg = LaunchConfiguration("world")
    headless = LaunchConfiguration("headless")
    start_sender = LaunchConfiguration("start_waypoint_sender")

    world_path = PathJoinSubstitution(
        [FindPackageShare("san_sim_gazebo"), "worlds", world_arg])

    gz_gui = ExecuteProcess(
        cmd=["gz", "sim", "-r", world_path], output="screen",
        condition=UnlessCondition(headless))
    gz_headless = ExecuteProcess(
        cmd=["gz", "sim", "-s", "-r", "--headless-rendering", world_path],
        output="screen", condition=IfCondition(headless))
    clock_bridge = Node(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="clock_bridge", output="screen",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"])
    wp_sender = Node(
        package="san_operator_tools", executable="waypoint_sender",
        name="waypoint_sender", output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(start_sender))

    return LaunchDescription([
        DeclareLaunchArgument("num_robots", default_value="8"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("world", default_value="empty_world.sdf"),
        DeclareLaunchArgument("start_waypoint_sender", default_value="true"),
        DeclareLaunchArgument("headless", default_value="false"),
        gz_gui,
        gz_headless,
        clock_bridge,
        TimerAction(period=5.0, actions=[wp_sender]),
        OpaqueFunction(function=launch_setup),
    ])
