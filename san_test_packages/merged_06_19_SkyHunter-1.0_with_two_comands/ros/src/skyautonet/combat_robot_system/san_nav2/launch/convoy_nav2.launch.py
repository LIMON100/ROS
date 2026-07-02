#!/usr/bin/env python3
# Per-robot nav2 for convoy followers — SINGLE SOURCE.
# Reads the ONE san_nav2/config/nav2_follower_params.yaml and injects the
# robot_<id>/ frame prefix at launch (no per-robot yaml fork), then brings up
# the nav2 stack under namespace robot_<id>.
import os
import tempfile

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace
from nav2_common.launch import RewrittenYaml
from launch.actions import (
      DeclareLaunchArgument, GroupAction, OpaqueFunction, TimerAction,
  ) 

def make_robot_nav2_params(ns, base_params):
    with open(base_params) as f:
        cfg = yaml.safe_load(f)
    base = f"{ns}/base_footprint"
    odom = f"{ns}/odom"
    
    def setp(node, key, val):
        cfg.get(node, {}).get("ros__parameters", {})[key] = val
    
    setp("bt_navigator", "robot_base_frame", base)
    bt_dir = os.path.join(get_package_share_directory("san_nav2"), "behavior_trees")
    setp("bt_navigator", "default_nav_to_pose_bt_xml",
        os.path.join(bt_dir, "single_plan_bt.xml"))
    setp("bt_navigator", "default_nav_through_poses_bt_xml",
        os.path.join(bt_dir, "way_plan_bt.xml"))
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
            pc["min_obstacle_height"] = 0.3
            pc["max_obstacle_height"] = 2.5
    out = os.path.join(tempfile.gettempdir(), f"convoy_{ns}_nav2_params.yaml")
    with open(out, "w") as f:
        yaml.safe_dump(cfg, f, default_flow_style=False)
    return out
    
    
def _nav(pkg, exe, params):
    return Node(package=pkg, executable=exe, name=exe, output="screen",
                parameters=[params])

def _setup(context, *args, **kwargs):
    ids = [s.strip() for s in
            LaunchConfiguration("robot_ids").perform(context).split(",") if s.strip()]
    base_params = os.path.join(
        get_package_share_directory("san_nav2"), "config", "nav2_params.yaml")
    actions = []
    for i, rid in enumerate(ids):
        ns = f"robot_{rid}"
        params = RewrittenYaml(
            source_file=make_robot_nav2_params(ns, base_params),
            root_key=ns, param_rewrites={"use_sim_time": "true"},
            convert_types=True)
        grp = GroupAction([
            PushRosNamespace(ns),
            _nav("nav2_controller", "controller_server", params),
            _nav("nav2_planner", "planner_server", params),
            _nav("nav2_behaviors", "behavior_server", params),
            _nav("nav2_bt_navigator", "bt_navigator", params),
            _nav("nav2_smoother", "smoother_server", params),
            _nav("nav2_waypoint_follower", "waypoint_follower", params),
            Node(package="nav2_lifecycle_manager", executable="lifecycle_manager",
                name="lifecycle_manager_navigation", output="screen",
                parameters=[{"use_sim_time": True, "autostart": True,
                            "node_names": ["controller_server", "planner_server",
                                            "behavior_server", "bt_navigator",
                                            "smoother_server", "waypoint_follower"]}]),

            Node(package="san_operator_tools", executable="convoy_nav2_follower",
                name="convoy_nav2_follower", output="screen",
                parameters=[{"use_sim_time": True, "robot_id": int(rid)}]),
            
        ])
        actions.append(TimerAction(period=float(i) * 8.0, actions=[grp]))  # stagger
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("robot_ids", default_value="2,3,4,5"),
        OpaqueFunction(function=_setup),
    ])
