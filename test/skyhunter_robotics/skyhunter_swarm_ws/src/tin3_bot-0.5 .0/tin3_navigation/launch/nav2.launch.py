# # =============================================================================
# # Nav2 Launch File — Tactical UGV (tin3_bot)
# #
# # Custom launch that starts ONLY the nodes we needed.

# # Launches:
# #   1. map_server + AMCL (localization)
# #   2. pointcloud_to_laserscan (3D → 2D for AMCL)
# #   3. Nav2 navigation nodes (controller, planner, BT, recoveries, smoother)
# #   4. Lifecycle managers
# #   5. RViz
# #
# # Usage:
# #   ros2 launch tin3_navigation nav2_launch.py
# #   ros2 launch tin3_navigation nav2_launch.py use_rviz:=false
# # =============================================================================

# import os

# from ament_index_python.packages import get_package_share_directory

# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument, GroupAction, SetEnvironmentVariable
# from launch.conditions import IfCondition
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node, LoadComposableNodes
# from launch_ros.descriptions import ComposableNode
# from launch_ros.actions import ComposableNodeContainer


# def generate_launch_description():
#     pkg_tin3_navigation = get_package_share_directory("tin3_navigation")
#     map_yaml = LaunchConfiguration("map")
#     nav2_params = LaunchConfiguration("params_file")
#     use_rviz = LaunchConfiguration("use_rviz")
    
 
#     declare_map = DeclareLaunchArgument(
#         "map",
#         default_value=os.path.join(
#             pkg_tin3_navigation, "maps", "empty_world_map.yaml"
#         ),
#     )
#     declare_params = DeclareLaunchArgument(
#         "params_file",
#         default_value=os.path.join(
#             pkg_tin3_navigation, "config", "nav2_params.yaml"
#         ),
#     )
#     declare_use_rviz = DeclareLaunchArgument(
#         "use_rviz", default_value="true",
#     )

#     # ==================== PointCloud to LaserScan ====================
#     pointcloud_to_laserscan = Node(
#         package="pointcloud_to_laserscan",
#         executable="pointcloud_to_laserscan_node",
#         name="pointcloud_to_laserscan",
#         parameters=[
#             {
#                 "use_sim_time": True,
#                 "target_frame": "base_footprint",
#                 "min_height": 0.1,
#                 "max_height": 1.5,
#                 "angle_min": -1.0472,
#                 "angle_max": 1.0472,
#                 "angle_increment": 0.00872,
#                 "scan_time": 0.1,
#                 "range_min": 0.1,
#                 "range_max": 30.0,
#                 "inf_epsilon": 1.0,
#                 "use_inf": True,
#                 "qos_overrides": {
#                     "/scan": {
#                         "publisher": {
#                             "reliability": "reliable",
#                         }
#                     }
#                 },
#             }
#         ],
#         remappings=[
#             ("cloud_in", "/scan/points"),
#             ("scan", "/scan"),
#         ],
#     )

#     # ==================== Localization Nodes ====================
#     map_server = Node(
#         package="nav2_map_server",
#         executable="map_server",
#         name="map_server",
#         output="screen",
#         parameters=[
#             nav2_params,
#             {"yaml_filename": map_yaml},
#         ],
#     )

#     amcl = Node(
#         package="nav2_amcl",
#         executable="amcl",
#         name="amcl",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     lifecycle_manager_localization = Node(
#         package="nav2_lifecycle_manager",
#         executable="lifecycle_manager",
#         name="lifecycle_manager_localization",
#         output="screen",
#         parameters=[
#             {"use_sim_time": True},
#             {"autostart": True},
#             {"node_names": ["map_server", "amcl"]},
#         ],
#     )

#     # ==================== Navigation Nodes ====================
#     controller_server = Node(
#         package="nav2_controller",
#         executable="controller_server",
#         name="controller_server",
#         output="screen",
#         parameters=[nav2_params],
#         remappings=[("cmd_vel", "cmd_vel_nav")],
#     )

#     smoother_server = Node(
#         package="nav2_smoother",
#         executable="smoother_server",
#         name="smoother_server",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     planner_server = Node(
#         package="nav2_planner",
#         executable="planner_server",
#         name="planner_server",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     behavior_server = Node(
#         package="nav2_behaviors",
#         executable="behavior_server",
#         name="behavior_server",
#         output="screen",
#         parameters=[nav2_params],
#     )
    
#     bt_navigator = Node(
#         package="nav2_bt_navigator",
#         executable="bt_navigator",
#         name="bt_navigator",
#         output="screen",
#         parameters=[nav2_params,{
#             "default_nav_to_pose_bt_xml": os.path.join(
#                 get_package_share_directory("nav2_bt_navigator"),
#                 "behavior_trees",
#                 "navigate_to_pose_w_replanning_and_recovery.xml"
#             ),
#             "default_nav_through_poses_bt_xml": os.path.join(
#                 get_package_share_directory("nav2_bt_navigator"),
#                 "behavior_trees",
#                 "navigate_through_poses_w_replanning_and_recovery.xml"
#             ),
#         },],
#     )

#     waypoint_follower = Node(
#         package="nav2_waypoint_follower",
#         executable="waypoint_follower",
#         name="waypoint_follower",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     velocity_smoother = Node(
#         package="nav2_velocity_smoother",
#         executable="velocity_smoother",
#         name="velocity_smoother",
#         output="screen",
#         parameters=[nav2_params],
#         remappings=[
#             ("cmd_vel", "cmd_vel_nav"),
#             ("cmd_vel_smoothed", "cmd_vel"),
#         ],
#     )

#     lifecycle_manager_navigation = Node(
#         package="nav2_lifecycle_manager",
#         executable="lifecycle_manager",
#         name="lifecycle_manager_navigation",
#         output="screen",
#         parameters=[
#             {"use_sim_time": True},
#             {"autostart": True},
#             {
#                 "node_names": [
#                     "controller_server",
#                     "smoother_server",
#                     "planner_server",
#                     "behavior_server",
#                     "bt_navigator",
#                     "waypoint_follower",
#                     "velocity_smoother",
#                 ]
#             },
#         ],
#     )

#     # ==================== RViz ====================
#     rviz_config = os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")

#     rviz_node = Node(
#         package="rviz2",
#         executable="rviz2",
#         name="rviz2",
#         arguments=["-d", rviz_config],
#         parameters=[{"use_sim_time": True}],
#         output="screen",
#         condition=IfCondition(use_rviz),
#     )

#     # ==================== Launch Description ====================
#     return LaunchDescription(
#         [
#             declare_map,
#             declare_params,
#             declare_use_rviz,
#             pointcloud_to_laserscan,
#             map_server,
#             amcl,
#             lifecycle_manager_localization,
#             controller_server,
#             smoother_server,
#             planner_server,
#             behavior_server,
#             bt_navigator,
#             waypoint_follower,
#             velocity_smoother,
#             lifecycle_manager_navigation,
#             rviz_node,
#         ]
#     )



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

# import os

# from ament_index_python.packages import get_package_share_directory

# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument
# from launch.conditions import IfCondition
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node


# def generate_launch_description():
#     pkg_tin3_navigation = get_package_share_directory("tin3_navigation")

#     nav2_params = LaunchConfiguration("params_file")
#     use_rviz = LaunchConfiguration("use_rviz")

#     declare_params = DeclareLaunchArgument(
#         "params_file",
#         default_value=os.path.join(
#             pkg_tin3_navigation, "config", "nav2_params.yaml"
#         ),
#     )
#     declare_use_rviz = DeclareLaunchArgument(
#         "use_rviz",
#         default_value="true",
#     )

#     # ==================== Static TF: map → odom (identity) ====================
#     # Replaces AMCL for mapless navigation.
#     # Remove this and add AMCL/SLAM when using a real map.
#     static_tf_map_odom = Node(
#         package="tf2_ros",
#         executable="static_transform_publisher",
#         name="static_tf_map_odom",
#         arguments=["0", "0", "0", "0", "0", "0", "map", "odom"],
#         parameters=[{"use_sim_time": True}],
#     )

#     # ==================== Navigation Nodes ====================
#     controller_server = Node(
#         package="nav2_controller",
#         executable="controller_server",
#         name="controller_server",
#         output="screen",
#         parameters=[nav2_params],
#         remappings=[("cmd_vel", "cmd_vel_nav")],
#     )

#     planner_server = Node(
#         package="nav2_planner",
#         executable="planner_server",
#         name="planner_server",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     smoother_server = Node(
#         package="nav2_smoother",
#         executable="smoother_server",
#         name="smoother_server",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     behavior_server = Node(
#         package="nav2_behaviors",
#         executable="behavior_server",
#         name="behavior_server",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     bt_navigator = Node(
#         package="nav2_bt_navigator",
#         executable="bt_navigator",
#         name="bt_navigator",
#         output="screen",
#         parameters=[
#             nav2_params,
#             {
#                 "default_nav_to_pose_bt_xml": os.path.join(
#                     get_package_share_directory("nav2_bt_navigator"),
#                     "behavior_trees",
#                     "navigate_to_pose_w_replanning_and_recovery.xml",
#                 ),
#                 "default_nav_through_poses_bt_xml": os.path.join(
#                     get_package_share_directory("nav2_bt_navigator"),
#                     "behavior_trees",
#                     "navigate_through_poses_w_replanning_and_recovery.xml",
#                 ),
#             },
#         ],
#     )

#     waypoint_follower = Node(
#         package="nav2_waypoint_follower",
#         executable="waypoint_follower",
#         name="waypoint_follower",
#         output="screen",
#         parameters=[nav2_params],
#     )

#     velocity_smoother = Node(
#         package="nav2_velocity_smoother",
#         executable="velocity_smoother",
#         name="velocity_smoother",
#         output="screen",
#         parameters=[nav2_params],
#         remappings=[
#             ("cmd_vel", "cmd_vel_nav"),
#             ("cmd_vel_smoothed", "cmd_vel"),
#         ],
#     )

#     lifecycle_manager_navigation = Node(
#         package="nav2_lifecycle_manager",
#         executable="lifecycle_manager",
#         name="lifecycle_manager_navigation",
#         output="screen",
#         parameters=[
#             {"use_sim_time": True},
#             {"autostart": True},
#             {
#                 "node_names": [
#                     "controller_server",
#                     "smoother_server",
#                     "planner_server",
#                     "behavior_server",
#                     "bt_navigator",
#                     "waypoint_follower",
#                     "velocity_smoother",
#                 ]
#             },
#         ],
#     )

#     # ==================== RViz ====================
#     rviz_config = os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")

#     rviz_node = Node(
#         package="rviz2",
#         executable="rviz2",
#         name="rviz2",
#         arguments=["-d", rviz_config],
#         parameters=[{"use_sim_time": True}],
#         output="screen",
#         condition=IfCondition(use_rviz),
#     )

#     # ==================== Launch Description ====================
#     return LaunchDescription(
#         [
#             declare_params,
#             declare_use_rviz,
#             static_tf_map_odom,
#             controller_server,
#             planner_server,
#             smoother_server,
#             behavior_server,
#             bt_navigator,
#             waypoint_follower,
#             velocity_smoother,
#             lifecycle_manager_navigation,
#             rviz_node,
#         ]
#     )

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml  # <--- CRITICAL IMPORT

def generate_launch_description():
    pkg_tin3_navigation = get_package_share_directory("tin3_navigation")
    
    # 1. Configuration Variables
    nav2_params_file = LaunchConfiguration("params_file")
    use_rviz = LaunchConfiguration("use_rviz")
    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')

    declare_params = DeclareLaunchArgument(
        "params_file", default_value=os.path.join(pkg_tin3_navigation, "config", "nav2_params.yaml"))
    declare_use_rviz = DeclareLaunchArgument("use_rviz", default_value="true")
    declare_namespace = DeclareLaunchArgument("namespace", default_value="")
    declare_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_autostart = DeclareLaunchArgument("autostart", default_value="true")

    # 2. Dynamic Frame Generation
    # If namespace is 'SH_02', frames become 'SH_02/odom', 'SH_02/base_footprint'
    # If namespace is empty, they stay 'odom', 'base_footprint'
    
    # We use PythonExpression to create the correct string
    odom_frame = PythonExpression(["'", namespace, "/odom' if '", namespace, "' != '' else 'odom'"])
    base_frame = PythonExpression(["'", namespace, "/base_footprint' if '", namespace, "' != '' else 'base_footprint'"])
    scan_topic = PythonExpression(["'/", namespace, "/scan/points' if '", namespace, "' != '' else '/scan/points'"])

    # 3. Create the RewrittenYaml
    # This magically replaces the keys in your YAML file with the variables above
    configured_params = RewrittenYaml(
        source_file=nav2_params_file,
        root_key=namespace,
        param_rewrites={
            'use_sim_time': use_sim_time,
            'autostart': autostart,
            'global_frame': 'map', # Global costmap always uses map
            'robot_base_frame': base_frame,
            'odom_frame': odom_frame,
            'topic': scan_topic,
            'scan_topic': scan_topic
        },
        convert_types=True
    )

    # 4. Nodes (Using configured_params instead of the raw file)
    
    # TF Publisher (Needed for the empty/namespaced logic)
    static_tf_map_odom = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="static_tf_map_odom",
        namespace=namespace,
        arguments=["0", "0", "0", "0", "0", "0", "map", odom_frame],
        parameters=[{"use_sim_time": use_sim_time}],
    )

    controller_server = Node(
        package="nav2_controller", executable="controller_server",
        name="controller_server", namespace=namespace, output="screen",
        parameters=[configured_params], # <--- USING REWRITTEN YAML
        remappings=[("cmd_vel", "cmd_vel_nav")],
    )

    planner_server = Node(
        package="nav2_planner", executable="planner_server",
        name="planner_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    smoother_server = Node(
        package="nav2_smoother", executable="smoother_server",
        name="smoother_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    behavior_server = Node(
        package="nav2_behaviors", executable="behavior_server",
        name="behavior_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    bt_navigator = Node(
        package="nav2_bt_navigator", executable="bt_navigator",
        name="bt_navigator", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    waypoint_follower = Node(
        package="nav2_waypoint_follower", executable="waypoint_follower",
        name="waypoint_follower", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    velocity_smoother = Node(
        package="nav2_velocity_smoother", executable="velocity_smoother",
        name="velocity_smoother", namespace=namespace, output="screen",
        parameters=[configured_params],
        remappings=[("cmd_vel", "cmd_vel_nav"), ("cmd_vel_smoothed", "cmd_vel")],
    )

    # Lifecycle Manager
    lifecycle_manager_navigation = Node(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager",
        namespace=namespace,
        output="screen",
        parameters=[
            {"use_sim_time": use_sim_time},
            {"autostart": autostart},
            {"bond_timeout": 60.0}, # High timeout for safety
            {"node_names": ["controller_server", "smoother_server", "planner_server", 
                            "behavior_server", "bt_navigator", "waypoint_follower", "velocity_smoother"]},
        ],
    )

    rviz_node = Node(
        package="rviz2", executable="rviz2", name="rviz2",
        arguments=["-d", os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")],
        parameters=[{"use_sim_time": use_sim_time}], output="screen", condition=IfCondition(use_rviz),
    )

    return LaunchDescription([
        declare_params, declare_use_rviz, declare_namespace, declare_sim_time, declare_autostart,
        static_tf_map_odom, controller_server, planner_server, smoother_server,
        behavior_server, bt_navigator, waypoint_follower, velocity_smoother,
        lifecycle_manager_navigation, rviz_node,
    ])