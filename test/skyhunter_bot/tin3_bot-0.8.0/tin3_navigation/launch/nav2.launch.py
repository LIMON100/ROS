# =============================================================================
# Nav2 Launch File — Tactical UGV (tin3_bot) — MAPLESS Configuration
# ROS 2 Humble
#
# Mode: Odom-only navigation (NO AMCL, NO map_server)
# Use case: Waypoint following in empty/open worlds
#
# Provides static map→odom identity transform so Nav2 TF tree is complete:
#   map → odom (static identity)
#   odom → base_footprint (from EKF or diff_drive)
#
# Launches:
#   1. Static TF: map → odom (identity)
#   2. Nav2 navigation nodes (controller, planner, BT, behaviors, smoother)
#   3. Waypoint follower
#   4. Velocity smoother
#   5. Lifecycle manager
#   6. RViz (optional)
#
# Usage:
#   ros2 launch tin3_navigation nav2_launch.py
#   ros2 launch tin3_navigation nav2_launch.py use_rviz:=false
# =============================================================================

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_tin3_navigation = get_package_share_directory("tin3_navigation")

    nav2_params = LaunchConfiguration("params_file")
    use_rviz = LaunchConfiguration("use_rviz")

    declare_params = DeclareLaunchArgument(
        "params_file",
        default_value=os.path.join(
            pkg_tin3_navigation, "config", "nav2_params.yaml"
        ),
    )
    declare_use_rviz = DeclareLaunchArgument(
        "use_rviz",
        default_value="true",
    )

    # ==================== Static TF: map → odom (identity) ====================
    # Replaces AMCL for mapless navigation.
    # Remove this and add AMCL/SLAM when using a real map.
    static_tf_map_odom = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="static_tf_map_odom",
        arguments=["0", "0", "0", "0", "0", "0", "map", "odom"],
        parameters=[{"use_sim_time": True}],
    )

    # ==================== Navigation Nodes ====================
    controller_server = Node(
        package="nav2_controller",
        executable="controller_server",
        name="controller_server",
        output="screen",
        parameters=[nav2_params],
        remappings=[("cmd_vel", "cmd_vel_nav")],
    )

    planner_server = Node(
        package="nav2_planner",
        executable="planner_server",
        name="planner_server",
        output="screen",
        parameters=[nav2_params],
    )

    smoother_server = Node(
        package="nav2_smoother",
        executable="smoother_server",
        name="smoother_server",
        output="screen",
        parameters=[nav2_params],
    )

    behavior_server = Node(
        package="nav2_behaviors",
        executable="behavior_server",
        name="behavior_server",
        output="screen",
        parameters=[nav2_params],
    )

    bt_navigator = Node(
        package="nav2_bt_navigator",
        executable="bt_navigator",
        name="bt_navigator",
        output="screen",
        parameters=[
            nav2_params,
            {
                "default_nav_to_pose_bt_xml": os.path.join(
                    get_package_share_directory("nav2_bt_navigator"),
                    "behavior_trees",
                    "navigate_to_pose_w_replanning_and_recovery.xml",
                ),
                "default_nav_through_poses_bt_xml": os.path.join(
                    get_package_share_directory("nav2_bt_navigator"),
                    "behavior_trees",
                    "navigate_through_poses_w_replanning_and_recovery.xml",
                ),
            },
        ],
    )

    waypoint_follower = Node(
        package="nav2_waypoint_follower",
        executable="waypoint_follower",
        name="waypoint_follower",
        output="screen",
        parameters=[nav2_params],
    )

    velocity_smoother = Node(
        package="nav2_velocity_smoother",
        executable="velocity_smoother",
        name="velocity_smoother",
        output="screen",
        parameters=[nav2_params],
        remappings=[
            ("cmd_vel", "cmd_vel_nav"),
            ("cmd_vel_smoothed", "cmd_vel"),
        ],
    )

    lifecycle_manager_navigation = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager_navigation",
        output="screen",
        parameters=[
            {"use_sim_time": True},
            {"autostart": True},
            {
                "node_names": [
                    "controller_server",
                    "smoother_server",
                    "planner_server",
                    "behavior_server",
                    "bt_navigator",
                    "waypoint_follower",
                    "velocity_smoother",
                ]
            },
        ],
    )

    # ==================== RViz ====================
    rviz_config = os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")

    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        arguments=["-d", rviz_config],
        parameters=[{"use_sim_time": True}],
        output="screen",
        condition=IfCondition(use_rviz),
    )

    # ==================== Launch Description ====================
    return LaunchDescription(
        [
            declare_params,
            declare_use_rviz,
            static_tf_map_odom,
            controller_server,
            planner_server,
            smoother_server,
            behavior_server,
            bt_navigator,
            waypoint_follower,
            velocity_smoother,
            lifecycle_manager_navigation,
            rviz_node,
        ]
    )