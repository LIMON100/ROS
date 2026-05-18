"""Thin wrapper that includes the EKF launch file.

Previously this file was empty (0 bytes), which made it ambiguous whether
localization was intended to be launched separately from ekf.launch.py.
Anything that needs to bring up localization standalone should target this
file; ekf.launch.py is still the canonical implementation.
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_nav = get_package_share_directory("skyhunter_navigation")

    namespace_arg = DeclareLaunchArgument("namespace", default_value="")

    ekf_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_nav, "launch", "ekf.launch.py")
        ),
        launch_arguments={"namespace": LaunchConfiguration("namespace")}.items(),
    )

    return LaunchDescription([namespace_arg, ekf_launch])
