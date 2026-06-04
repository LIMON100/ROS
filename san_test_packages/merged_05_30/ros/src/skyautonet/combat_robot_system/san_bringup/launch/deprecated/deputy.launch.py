# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

r"""
Deputy UGV — fixed-role launch wrapper (DCN-2026-011 D-034).

Equivalent to:
  ros2 launch san_bringup squadron.launch.py \\
      robot_id:=3 robot_role:=deputy sbc_id:=0 \\
      hub_features:=false include_regression:=auto

The Deputy UGV runs as a single-SBC backup that can promote into Hub
on watchdog timeout (see san_role_management HubRoleManager). hub_features
stays false here — the Deputy enables them only after a successful
promotion, not at boot.
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    deployment_mode = LaunchConfiguration("deployment_mode")
    include_regression = LaunchConfiguration("include_regression")

    squadron = PathJoinSubstitution([
        FindPackageShare("san_bringup"), "launch", "squadron.launch.py",
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            "deployment_mode", default_value="production",
            description="production | demo | lab_test | bench | development",
        ),
        DeclareLaunchArgument(
            "include_regression", default_value="auto",
            description="auto | true | false",
        ),
        LogInfo(msg=["[deputy] Launching Deputy UGV profile"]),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id":            "3",
                "robot_role":          "deputy",
                "sbc_id":              "0",
                "hub_features":        "false",
                "include_regression":  include_regression,
                "deployment_mode":     deployment_mode,
            }.items(),
        ),
    ])
