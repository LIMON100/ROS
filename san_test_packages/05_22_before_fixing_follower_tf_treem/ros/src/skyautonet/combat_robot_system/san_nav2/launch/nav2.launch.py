# """SAN v1.5 — Nav2 navigation stack.

# Brings up Nav2 (planner + controller + behaviors + lifecycle manager)
# with the SAN-tuned params. Pairs with san_localization for pose.

# Usage:
#     ros2 launch san_nav2 nav2.launch.py
#     ros2 launch san_nav2 nav2.launch.py params_file:=...
# """
# import os

# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import (
#     DeclareLaunchArgument, IncludeLaunchDescription,
# )
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration


# def generate_launch_description():
#     pkg_share = get_package_share_directory("san_nav2")
#     nav2_bringup = get_package_share_directory("nav2_bringup")

#     return LaunchDescription([
#         DeclareLaunchArgument("use_sim_time", default_value="true"),
#         DeclareLaunchArgument(
#             "params_file",
#             default_value=os.path.join(pkg_share, "config", "nav2_params.yaml")),
#         DeclareLaunchArgument(
#             "autostart", default_value="true"),

#         # Include the standard Nav2 bringup with our params
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource(
#                 os.path.join(nav2_bringup, "launch", "navigation_launch.py"),
#             ),
#             launch_arguments={
#                 "use_sim_time": LaunchConfiguration("use_sim_time"),
#                 "params_file":  LaunchConfiguration("params_file"),
#                 "autostart":    LaunchConfiguration("autostart"),
#             }.items(),
#         ),
#     ])


# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node
# from nav2_common.launch import RewrittenYaml

# def generate_launch_description():
#     pkg_share = get_package_share_directory("san_nav2")
#     nav2_bringup = get_package_share_directory("nav2_bringup")

#     params_file = LaunchConfiguration("params_file")
#     use_sim_time = LaunchConfiguration("use_sim_time")
#     autostart = LaunchConfiguration("autostart")

#     # This creates a remapped parameter file that handles namespaces properly
#     configured_params = RewrittenYaml(
#         source_file=params_file,
#         param_rewrites={'use_sim_time': use_sim_time},
#         convert_types=True
#     )

#     return LaunchDescription([
#         DeclareLaunchArgument("use_sim_time", default_value="true"),
#         DeclareLaunchArgument("params_file", default_value=os.path.join(pkg_share, "config", "nav2_params.yaml")),
#         DeclareLaunchArgument("autostart", default_value="true"),

#         # 1. Standard Nav2 (Planner, Controller, etc.)
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource(os.path.join(nav2_bringup, "launch", "navigation_launch.py")),
#             launch_arguments={
#                 "use_sim_time": use_sim_time,
#                 "params_file":  params_file,
#                 "autostart":    autostart,
#             }.items(),
#         ),
#     ])



import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    pkg_share    = get_package_share_directory("san_nav2")
    nav2_bringup = get_package_share_directory("nav2_bringup")

    params_file  = LaunchConfiguration("params_file")
    use_sim_time = LaunchConfiguration("use_sim_time")
    autostart    = LaunchConfiguration("autostart")
    namespace    = LaunchConfiguration("namespace")

    odom_frame = PythonExpression(
        ["'", namespace, "/odom' if '", namespace, "' != '' else 'odom'"]
    )
    base_frame = PythonExpression(
        ["'", namespace, "/base_footprint' if '", namespace, "' != '' else 'base_footprint'"]
    )

    configured_params = RewrittenYaml(
        source_file=params_file,
        root_key=namespace,
        param_rewrites={
            "use_sim_time":     use_sim_time,
            "global_frame":     "map",           # nav2 always plans in map
            "robot_base_frame": base_frame,
            "odom_frame":       odom_frame,
        },
        convert_types=True,
    )

    return LaunchDescription([
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        DeclareLaunchArgument("namespace",    default_value=""),
        DeclareLaunchArgument("params_file",
            default_value=os.path.join(pkg_share, "config", "nav2_params.yaml")),
        DeclareLaunchArgument("autostart",    default_value="true"),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup, "launch", "navigation_launch.py")),
            launch_arguments={
                "use_sim_time": use_sim_time,
                "namespace":    namespace,
                "params_file":  configured_params,
                "autostart":    autostart,
            }.items(),
        ),
    ])
