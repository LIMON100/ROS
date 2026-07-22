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
# import os

# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument, GroupAction
# from launch.conditions import IfCondition
# from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
# from launch_ros.actions import Node
# from launch_ros.substitutions import FindPackageShare


# def generate_launch_description():
#     pkg_share = get_package_share_directory("san_localization")
#     use_sim_time = LaunchConfiguration("use_sim_time", default="true")
#     use_amcl     = LaunchConfiguration("use_amcl", default="false")
#     map_name     = LaunchConfiguration("map", default="empty_world_map.yaml")

#     map_yaml_path = PathJoinSubstitution([
#         FindPackageShare("san_localization"), "maps", map_name,
#     ])

#     return LaunchDescription([
#         DeclareLaunchArgument("use_sim_time", default_value="true"),
#         DeclareLaunchArgument("use_amcl", default_value="false"),
#         DeclareLaunchArgument("map", default_value="empty_world_map.yaml"),

#         # ─── EKF Local (odom → base_link) ───────────────────────────
#         Node(
#             package="robot_localization",
#             executable="ekf_node",
#             name="ekf_filter_node_local",
#             output="screen",
#             parameters=[
#                 os.path.join(pkg_share, "config", "ekf_local.yaml"),
#                 {"use_sim_time": use_sim_time,
#                  "wait_for_datum": True, 
#                   "datum": [37.558833, 126.979666, 0.0]
#                  },
#             ],
#             remappings=[
#                 ("odometry/filtered", "odometry/filtered/local"),
#             ],
#         ),

#         # ─── EKF Global (map → odom) ────────────────────────────────
#         Node(
#             package="robot_localization",
#             executable="ekf_node",
#             name="ekf_filter_node_global",
#             output="screen",
#             parameters=[
#                 os.path.join(pkg_share, "config", "ekf_global.yaml"),
#                 {"use_sim_time": use_sim_time},
#             ],
#             remappings=[
#                 ("odometry/filtered", "odometry/filtered"),
#             ],
#         ),

#         # ─── navsat_transform ──────────────────────────────────────
#         # Node(
#         #     package="robot_localization",
#         #     executable="navsat_transform_node",
#         #     name="navsat_transform_node",
#         #     output="screen",
#         #     parameters=[
#         #         os.path.join(pkg_share, "config", "ekf_global.yaml"),
#         #         {"use_sim_time": use_sim_time},
#         #     ],
#         #     remappings=[
#         #         ("imu", "imu/data"),
#         #         ("gps/fix", "gps/fix"),
#         #         ("odometry/filtered", "odometry/filtered"),
#         #         ("odometry/gps", "odometry/gps"),
#         #     ],
#         # ),

#         Node(
#             package="robot_localization",
#             executable="navsat_transform_node",
#             name="navsat_transform_node",
#             output="screen",
#             parameters=[
#                 os.path.join(pkg_share, "config", "ekf_global.yaml"),
#                 {
#                     "use_sim_time": use_sim_time,
#                     "wait_for_datum": True, 
#                     "datum": [37.558833, 126.979666, 0.0],
#                     "publish_filtered_gps": True
#                 },
#             ],
#             remappings=[
#                 ("imu", "imu/data"),
#                 ("gps/fix", "gps/fix"),
#                 # CHANGE THIS LINE: Use local instead of global to break the loop
#                 ("odometry/filtered", "odometry/filtered"), 
#                 ("odometry/gps", "odometry/gps"),
#             ],
#         ),

#         # ─── (선택) Map server + AMCL ──────────────────────────────
#         GroupAction(
#             condition=IfCondition(use_amcl),
#             actions=[
#                 Node(
#                     package="nav2_map_server",
#                     executable="map_server",
#                     name="map_server",
#                     output="screen",
#                     parameters=[{
#                         "use_sim_time": use_sim_time,
#                         "yaml_filename": map_yaml_path,
#                     }],
#                 ),
#                 Node(
#                     package="nav2_amcl",
#                     executable="amcl",
#                     name="amcl",
#                     output="screen",
#                     parameters=[
#                         PathJoinSubstitution([
#                             FindPackageShare("san_nav2"),
#                             "config", "nav2_params.yaml",
#                         ]),
#                         {"use_sim_time": use_sim_time},
#                     ],
#                 ),
#                 Node(
#                     package="nav2_lifecycle_manager",
#                     executable="lifecycle_manager",
#                     name="lifecycle_manager_localization",
#                     output="screen",
#                     parameters=[{
#                         "use_sim_time": use_sim_time,
#                         "autostart": True,
#                         "node_names": ["map_server", "amcl"],
#                     }],
#                 ),
#             ],
#         ),
#     ])


import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    pkg_share = get_package_share_directory("san_localization")
    ekf_config = os.path.join(pkg_share, "config", "ekf.yaml")
    
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")
    map_name = LaunchConfiguration("map", default="tile3.yaml")
    map_yaml_path = PathJoinSubstitution([FindPackageShare("san_localization"), "maps", map_name])

    return LaunchDescription([
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("map", default_value="tile3.yaml"),

        # 1. Your OLD Working EKF (Single Node Mode)
        Node(
            package="robot_localization",
            executable="ekf_node",
            name="ekf_filter_node",
            output="screen",
            parameters=[ekf_config, {"use_sim_time": use_sim_time}],
            # REMAP: Ensure this matches what Nav2 expects (usually /odometry/filtered)
            remappings=[("odometry/filtered", "/odometry/filtered")]
        ),

        # 2. Your OLD Working NavSat Transform
        Node(
            package="robot_localization",
            executable="navsat_transform_node",
            name="navsat_transform",
            output="screen",
            parameters=[ekf_config, {"use_sim_time": use_sim_time}],
            remappings=[
                ("gps/fix", "gps/fix"),
                ("imu", "imu/data"),
                ("odometry/gps", "odometry/gps"),
                ("odometry/filtered", "/odometry/filtered"),
            ],
        ),

        # 3. RESTORE THE MAP SERVER (The missing piece!)
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

        # 4. NAV2 LIFECYCLE MANAGER (To "wake up" the map)
        Node(
            package="nav2_lifecycle_manager",
            executable="lifecycle_manager",
            name="lifecycle_manager_localization",
            output="screen",
            parameters=[{
                "use_sim_time": use_sim_time,
                "autostart": True,
                "node_names": ["map_server"],
            }],
        ),

        # 5. STATIC TF BRIDGE (Guarantees map connection in simulation)
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='map_to_odom_bridge',
            arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom']
        )
    ])