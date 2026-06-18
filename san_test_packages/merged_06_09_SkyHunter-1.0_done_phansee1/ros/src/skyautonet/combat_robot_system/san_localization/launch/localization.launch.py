# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — Localization launch (Dual-EKF + AMCL + Map Server).

Brings up:
  - ekf_filter_node_local        (odom → base_link)
  - ekf_filter_node_global       (map → odom)
  - navsat_transform_node        (NavSatFix → odometry/gps)
  - map_server                   (선택, use_amcl=true 시)
  - amcl                         (선택, use_amcl=true 시)
  - lifecycle_manager            (AMCL/map_server 라이프사이클 관리)

Usage:
    # GPS + IMU + wheel (실외, RTK 가능):
    ros2 launch san_localization localization.launch.py

    # 추가로 AMCL 활성화 (실내 / 매핑된 환경):
    ros2 launch san_localization localization.launch.py \\
        use_amcl:=true map:=empty_world_map.yaml
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.conditions import IfCondition
from launch.substitutions import (
    LaunchConfiguration, PathJoinSubstitution, PythonExpression,
)
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = get_package_share_directory("san_localization")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")
    use_amcl     = LaunchConfiguration("use_amcl", default="false")
    map_name     = LaunchConfiguration("map", default="empty_world_map.yaml")

    map_yaml_path = PathJoinSubstitution([
        FindPackageShare("san_localization"), "maps", map_name,
    ])

    return LaunchDescription([
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("use_amcl", default_value="false"),
        DeclareLaunchArgument("map", default_value="empty_world_map.yaml"),

        # ─── EKF Local (odom → base_link) ───────────────────────────
        Node(
            package="robot_localization",
            executable="ekf_node",
            name="ekf_filter_node_local",
            output="screen",
            parameters=[
                os.path.join(pkg_share, "config", "ekf_local.yaml"),
                {"use_sim_time": use_sim_time},
            ],
            remappings=[
                ("odometry/filtered", "odometry/filtered/local"),
            ],
        ),

        # ─── EKF Global (map → odom) ────────────────────────────────
        Node(
            package="robot_localization",
            executable="ekf_node",
            name="ekf_filter_node_global",
            output="screen",
            parameters=[
                os.path.join(pkg_share, "config", "ekf_global.yaml"),
                {"use_sim_time": use_sim_time},
                # LOC-1: AMCL also publishes map->odom. When use_amcl=true,
                # AMCL owns that TF, so the global EKF must NOT publish it
                # too (two publishers of map->odom fight, corrupting the
                # tree). publish_tf = (use_amcl != true). This overrides
                # ekf_global.yaml's publish_tf:true.
                {"publish_tf": ParameterValue(
                    PythonExpression(["'", use_amcl, "' != 'true'"]),
                    value_type=bool)},
            ],
            remappings=[
                ("odometry/filtered", "odometry/filtered"),
            ],
        ),

        # ─── navsat_transform ──────────────────────────────────────
        Node(
            package="robot_localization",
            executable="navsat_transform_node",
            name="navsat_transform_node",
            output="screen",
            parameters=[
                os.path.join(pkg_share, "config", "ekf_global.yaml"),
                {"use_sim_time": use_sim_time},
            ],
            remappings=[
                ("imu", "imu/data"),
                ("gps/fix", "gps/fix"),
                ("odometry/filtered", "odometry/filtered"),
                ("odometry/gps", "odometry/gps"),
            ],
        ),

        # ─── (선택) Map server + AMCL ──────────────────────────────
        GroupAction(
            condition=IfCondition(use_amcl),
            actions=[
                Node(
                    package="nav2_map_server",
                    executable="map_server",
                    name="map_server",
                    output="screen",
                    parameters=[{
                        "use_sim_time": use_sim_time,
                        "yaml_filename": map_yaml_path,
                    }],
                ),
                Node(
                    package="nav2_amcl",
                    executable="amcl",
                    name="amcl",
                    output="screen",
                    parameters=[
                        PathJoinSubstitution([
                            FindPackageShare("san_nav2"),
                            "config", "nav2_params.yaml",
                        ]),
                        {"use_sim_time": use_sim_time},
                    ],
                ),
                Node(
                    package="nav2_lifecycle_manager",
                    executable="lifecycle_manager",
                    name="lifecycle_manager_localization",
                    output="screen",
                    parameters=[{
                        "use_sim_time": use_sim_time,
                        "autostart": True,
                        "node_names": ["map_server", "amcl"],
                    }],
                ),
            ],
        ),
    ])
